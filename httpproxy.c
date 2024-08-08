#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <err.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h> 
#include <pthread.h>
#include <stdbool.h>

#include "httpproxy.h"
#include "queue.h"
#include "cache.h"

#define COMMANDS "N:R:s:m:"
#define BUF_SIZE 4096

int requestCounter = 0;
int numServers = 0;
int *connfds; 
File *files;
Queue *requests;
cacheQueue *cache;
pthread_t *pool;
pthread_mutex_t queueLock = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t requestsLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t serverLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queueCondition = PTHREAD_COND_INITIALIZER;
pthread_mutex_t cacheLock = PTHREAD_MUTEX_INITIALIZER; 
struct Arguments args;
struct Server *servers; 


int valid_nonzero_int(char number[]) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT32_MAX || *last != '\0') {
        return -1;
    }
    return num;
}


int valid_int(char number[]) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num < 0 || num > UINT32_MAX || *last != '\0') {
        return -1;
    }
    return num;
}

int validate_fileName(char *string) {
    char *language = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";
    char *ptr;
    for (int i=0; i < strlen(string); i++) {
        ptr = strchr(language, string[i]);
        if (!ptr) {
            return -1;
        }
    }
    return 0;
}


void error_400(int connfd) {
    char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
    send(connfd, response, strlen(response), 0);
    close(connfd);
}


void error_500(int connfd) {
    char *response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal Server Error\n";
    send(connfd, response, strlen(response), 0); 
    close(connfd); 
}


void error_501(int connfd) {
    char *response = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n";
    send(connfd, response, strlen(response), 0);
    close(connfd);
}


int create_listen_socket(uint16_t port) {
  struct sockaddr_in addr;
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    err(EXIT_FAILURE, "socket error");
  }

  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htons(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(listenfd, (struct sockaddr*)&addr, sizeof addr) < 0) {
    err(EXIT_FAILURE, "bind error");
  }

  if (listen(listenfd, 500) < 0) {
    err(EXIT_FAILURE, "listen error");
  }

  return listenfd;
}


/**
   Creates a socket for connecting to a server running on the same
   computer, listening on the specified port number.  Returns the
   socket file descriptor on succes.  On failure, returns -1 and sets
   errno appropriately.
 */
int create_client_socket(uint16_t port) {
  int clientfd = socket(AF_INET, SOCK_STREAM, 0);
  if (clientfd < 0) {
    return -1;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (connect(clientfd, (struct sockaddr*) &addr, sizeof addr)) {
    return -1;
  }
  return clientfd;
}


void healthcheck_servers(struct Server servers[]) {
    char request[1024];
    char buf[4096];
    int response;
    int failures;
    int requests;

    for (int i=0; i < numServers; i++) {
        sprintf(request, "GET /healthcheck HTTP/1.1\r\nHost: localhost:%d\r\n\r\n", servers[i].port);
        int connfd = create_client_socket(servers[i].port);
        if (connfd == -1) {
            printf("Server at index %d is down\n", i);
            servers[i].status = -1;
            close(connfd);
            continue;
        }

        int bytesSent = send(connfd, request, strlen(request), 0);
        int bytesRecieved = recv(connfd, buf, 4096, 0);
        int scan = sscanf(buf, "HTTP/1.1 %d OK\r\n", &response);

        if ((scan != 1) || (response != 200)) {
            printf("Server at index %d is down\n", i);
            servers[i].status = -1;
            close(connfd);
            continue; 
        }

        char *end = strstr(buf, "\r\n\r\n"); 
        int headerSize = end - buf; 
        scan = sscanf(buf+headerSize, "%d\n%d\n", &failures, &requests);
        requests++;
        if (scan != 2) {
            printf("Server at index %d is down\n", i);
            servers[i].status = -1;
            close(connfd);
            continue;
        }

        printf("Server %d: %d failures in %d requests\n", i, failures, requests);
        servers[i].failures = failures; 
        servers[i].requests = requests;
        servers[i].status = 1;
        

        close(connfd); 
    }
}


int optimal_server(struct Server servers[]) {
    int minIndex = -1;
    for (int i=0; i < numServers; i++) {
        if (servers[i].status != -1) {
            minIndex = i;
            break;
        }
    }
    if (minIndex == -1) {
        return -1; 
    }
    for (int i=0; i < numServers; i++) {
        if (servers[i].status == -1) {
            continue;
        }
        if (servers[minIndex].requests > servers[i].requests) {
            minIndex = i;
        }
        if (servers[minIndex].requests == servers[i].requests) {
            if (servers[minIndex].failures > servers[i].failures) {
                minIndex = i; 
            }
        }
    }
    return minIndex;
}


int proxy_request(int connfd, struct Server servers[], int optimalServerIndex) {
    
    char clientRequest[4096];
    char proxyRequest[4096];
    char buf[4096];
    char fileName[1000];
    char httpVersion[1000];
    char proxyHost[1000];
    char serverHost[1000];  
    char response[100];
    char body[4096]; 
    char modifiedTime[100]; 
    int responseCode;
    int contentLength;
    int failure;

    // Recieve Client->Proxy request into clientRequest buffer, and validate the buffers fields
    recv(connfd, clientRequest, BUF_SIZE, 0);
    printf("------CLIENT->PROXY BEFORE VALIDATING-----\n");
    printf("%s\n", clientRequest); 

    // Update our proxies total request counter
    pthread_mutex_lock(&serverLock);
    requestCounter++;
    printf("\t%d total proxied requests\n", requestCounter);
    if (requestCounter % args.healthchecks == 0) {
        printf("\tHealthcheck probe servers\n");
        healthcheck_servers(servers); 
    }
    pthread_mutex_unlock(&serverLock);

    // Validate Request
    int scan = sscanf(clientRequest, "GET /%s %s\nHost: %s\n", fileName, httpVersion, proxyHost);
    if (scan != 3) {
        error_501(connfd); 
        memset(fileName, 0, 1000);
        memset(httpVersion, 0, 1000);
        memset(proxyHost, 0, 1000);
        return -1;
    }
    if ((strlen(fileName) > 19) || (strcmp(httpVersion, "HTTP/1.1") != 0) || (validate_fileName(fileName) == -1)) {
        error_400(connfd);
        memset(fileName, 0, 1000);
        memset(httpVersion, 0, 1000);
        memset(proxyHost, 0, 1000);
        return -1;
    }

    sprintf(proxyRequest, "GET /%s %s\r\nHost: %s\r\n\r\n", fileName, httpVersion, proxyHost); 
    printf("------CLIENT->PROXY AFTER VALIDATING-----\n");
    printf("%s\n", proxyRequest); 

    // Before handing off work to Server, check that file is not already cached
    if (args.caching != -1) {
        pthread_mutex_lock(&cacheLock);
        printf("CHECKING CACHE\n");
        int cached = in_cache(cache, args, fileName, proxyHost, connfd);
        pthread_mutex_unlock(&cacheLock);
        if (cached == 1) {
            return 0;
        }   
    }

    // Request is so far valid and uncached, forward it to optimal server
    int proxyfd = create_client_socket(servers[optimalServerIndex].port);
    send(proxyfd, proxyRequest, BUF_SIZE, 0);

    // Recieve and store server -> proxy response in buf
    int bytesRecv = recv(proxyfd, buf, BUF_SIZE, 0);
    int bytesSent;

    // Store size of header (data upto double CLRF) in headerSize variable
    char *end = strstr(buf, "\r\n\r\n"); 
    int headerSize = end - buf + 4;

    // Parse out response code, could be only TWO potential formats. If error code, mark that request was faillure.
    int s = sscanf(buf, "HTTP/1.1 %d %s\r\nContent-Length: %d\r\nLast-Modified: %[^\r\n]s", &responseCode, response, &contentLength, modifiedTime); 
    if (s != 3) {
        int t = sscanf(buf, "HTTP/1.1 %d %s %s\r\nContent-Length: %d\r\nLast-Modified: %[^\r\n]s", &responseCode, response, response, &contentLength, modifiedTime);
    }
    if ((responseCode != 200) && (responseCode != 201)) {
        failure = 1;
    }

    // Build File struct, in order to populate file->body while we recv() from server if we are caching.
    struct File file;
    if (args.caching != -1) {
        memcpy(file.fileName, fileName, 20);
        memcpy(file.lastModified, modifiedTime, 100);
        file.body = malloc(args.cacheSpace * sizeof(char)); 
        file.bodySize = 0;
        file.serverPort = servers[optimalServerIndex].port; 
    }

    // Case 1: The response from SERVER->PROXY included both header data and body data
    if (headerSize != bytesRecv) {
        printf("\tCASE 1...\n");
        send(connfd, buf, bytesRecv, 0); 
        bytesSent = bytesRecv - headerSize;

        // Manually send the body portion of our buffer to our File struct
        if (args.caching != -1) {
            int newSize = file.bodySize + bytesSent;
            if (newSize <= args.cacheSpace) {
                memcpy(file.body, buf+headerSize, bytesSent);
            }
            file.bodySize = newSize;
        }
    } 

    // Case 2: The response from SERVER->PROXY included only the header data in the response
    else {
        printf("\tCASE 2...\n");
        send(connfd, buf, bytesRecv, 0);
        bytesSent = 0;
    }

    // After handling inital recv() cases where body is or is not present, loop through the file until
    // we have processed as many bytes as specified by the server.
    while (bytesSent != contentLength) {
        bytesRecv = recv(proxyfd, body, BUF_SIZE, 0);
        bytesSent += send(connfd, body, bytesRecv, 0);
        // As we process our buffers, if they fit in our file.body field, apppend the buffer to it
        int newSize = file.bodySize + bytesRecv;
        if (newSize <= args.cacheSpace) {
            memcpy(file.body+file.bodySize, body, bytesRecv);
        }
        file.bodySize = newSize; 
    }

    // We have now sent our built File struct off to the cache to see if it can be stored in it.
    if ((args.caching != -1) && (failure != 1)) {
        pthread_mutex_lock(&cacheLock);
        insert_cache(cache, args, file);
        pthread_mutex_unlock(&cacheLock);
    }

    // Update field of current Server struct requests and failures counters
    pthread_mutex_lock(&serverLock);
    printf("\tServer at index %d has ", optimalServerIndex);
    if (failure) {
        servers[optimalServerIndex].failures++;
    }
    printf("%d total requests and %d failures\n", servers[optimalServerIndex].requests, servers[optimalServerIndex].failures);
    pthread_mutex_unlock(&serverLock); 

    close(proxyfd);
    close(connfd); 
    
    memset(proxyRequest, 0, BUF_SIZE);
    memset(buf, 0, BUF_SIZE);
    memset(body, 0, BUF_SIZE); 
    memset(fileName, 0, 1000);
    memset(httpVersion, 0, 1000);
    memset(proxyHost, 0, 1000); 
    memset(serverHost, 0, 1000); 
    memset(response, 0, 100);
    memset(modifiedTime, 0, 100); 
}

void *worker(void *thread) {

    while(true) {  
        pthread_mutex_lock(&queueLock);
        int connfd = dequeue(requests);
        if (connfd == -1) {
            pthread_cond_wait(&queueCondition, &queueLock);
            connfd = dequeue(requests);
        }
        pthread_mutex_unlock(&queueLock);

        if (connfd != -1) {

            pthread_mutex_lock(&serverLock);
            int serverIndex = optimal_server(servers);
            if (serverIndex == -1) {
                error_500(connfd);
                pthread_mutex_unlock(&serverLock); 
                continue; 
            }
            for (int i=0; i < numServers; i++) {
                int proxyConnection = create_client_socket(servers[serverIndex].port);
                if (proxyConnection != -1) {
                    close(proxyConnection);
                    break;
                }
                servers[serverIndex].status = -1;
                serverIndex = optimal_server(servers);
                close(proxyConnection); 
            }
            if (serverIndex == -1) {
                error_500(connfd); 
                pthread_mutex_unlock(&serverLock); 
                continue; 
            }
            printf("Server at index %d identified as optimal server\n", serverIndex);
            servers[serverIndex].requests++; 
            pthread_mutex_unlock(&serverLock); 
            proxy_request(connfd, servers, serverIndex);  
        }
    }
}


void build_dispatcher(struct Arguments args) {

    requests = initializeQueue(1000, connfds);
    if (args.caching != -1) {
        cache = initialize_cacheQueue(args.cacheSize, files);
    }

    // Initialize and construct thread pool
    pool = (pthread_t *)malloc(sizeof(pthread_t) * args.connections);
    for (long i=0; i < args.connections; i++) {
        int ret = pthread_create(&pool[i], NULL, worker, (void *)i);
        if (ret) {
            errx(EXIT_FAILURE, "pthread_create() error %d", ret); 
        }
    }

    // Open listening socket and listen
    int listenfd = create_listen_socket(args.proxyPort);
    while (true) {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            warn("accept error");
            continue;
        }

        pthread_mutex_lock( &queueLock );
        enqueue(requests, connfd); 
        pthread_cond_signal( &queueCondition);
        pthread_mutex_unlock( &queueLock);
    }
}



int main(int argc, char *argv[]) {

    int proxyPort, commands = 1;
    int c;

    args.connections = 5;
    args.healthchecks = 5;
    args.cacheSize = 3;
    args.cacheSpace = 1024;
    args.caching = 1; 
    
    while ((c = getopt(argc, argv, COMMANDS)) != -1) {
        switch(c) {
            case 'N':
                args.connections = valid_nonzero_int(optarg);
                if (args.connections == -1) {
                    errx(EXIT_FAILURE, "invalid number of connections");
                }
                commands += 2;
                break; 
            case 'R':
                args.healthchecks = valid_nonzero_int(optarg);
                if (args.healthchecks == -1) {
                    errx(EXIT_FAILURE, "invalid interval for healthcheck");
                }
                commands += 2; 
                break; 
            case 's':
                args.cacheSize = valid_int(optarg);
                if (args.cacheSize == -1) {
                    errx(EXIT_FAILURE, "invalid size of cache");
                }
                if (args.cacheSize == 0) {
                    args.caching = -1; 
                }
                commands += 2;
                break; 
            case 'm':
                args.cacheSpace = valid_int(optarg);
                if (args.cacheSpace == -1) {
                    errx(EXIT_FAILURE, "invalid space for cache");
                }
                if (args.cacheSpace == 0) {
                    args.caching = -1; 
                }
                commands += 2;
                break; 
            case '?':
                errx(EXIT_FAILURE, "unrecognized command or missing option-value");
                break; 
        }  
    }


    // Assert that proxy port is provided and valid
    if ((argv[optind] == NULL) || (valid_int(argv[optind]) == -1)) {
        errx(EXIT_FAILURE, "missing or invalid inital port number to proxy from");
    } else {
        args.proxyPort = valid_int(argv[optind]);
        printf("Proxy port %d recieved\n", args.proxyPort); 
        optind++;
        commands++;
    }


    numServers = argc - commands;
    if (numServers == 0) {
        errx(EXIT_FAILURE, "at least one server port must be provided"); 
    }

    servers = (Server *)malloc(numServers * sizeof(struct Server));
    for (int i=0; i < numServers; i++) {
        servers[i].ID = i;
        int port = valid_int(argv[optind]);
        if (port == -1) {
            errx(EXIT_FAILURE, "invalid server port");
        } else {
            servers[i].port = port;
        }
        printf("Server stored in array at index %d with port %d\n", i, servers[i].port); 
        optind++;
    }

    healthcheck_servers(servers); 
    build_dispatcher(args); 

}