#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define __USE_XOPEN
#define _GNU_SOURCE
#include <time.h>

#include "cache.h"
#include "httpproxy.h"

typedef struct cacheQueue {
    int head;
    int tail;
    int length;
    int count;
    File *arr;
} cacheQueue;


cacheQueue *initialize_cacheQueue(int n, File *arr) {
    struct cacheQueue* q = (cacheQueue *)malloc(sizeof(struct cacheQueue));
    q->head = 0;
    q->tail = 0;
    q->length = n;
    q->count = 0;
    q->arr = (File *)malloc(n * sizeof(File));
    return q;
}   


int enqueue_cache(cacheQueue *q, File file) {
    if (q->count >= q->length) {
        dequeue_cache(q);
    }
    q->arr[q->tail] = file;
    q->tail = (q->tail + 1) % q->length;
    q->count++;
    return 0; 
}


void dequeue_cache(cacheQueue *q) {
    struct File del = q->arr[q->head];
   // memset(del.body, 0, 1000);
   // memset(del.fileName, 0, 20);
    q->head = (q->head + 1) % q->length;
    q->count--;
}


void print_cacheQueue(cacheQueue *q, struct Arguments args) {
    printf("cacheQueue...\n");
    for (int i=0; i < args.cacheSize; i++) {
        printf("%s", q->arr[i].fileName);
    }
    printf("\n"); 
}



void insert_cache(struct cacheQueue *q, struct Arguments args, struct File file) {
    // If our constructed File object is larger than our cacheSpace. return
    if (file.bodySize > args.cacheSpace) {
        return;
    }
    // Otherwise store file in Cache, where enqueue functionality will bump other files accordingly
    enqueue_cache(q, file);
    print_cacheQueue(q, args); 
}


int compare_times(char *time1, char *time2) {

    printf("Comparing ...\n");
    printf("\t%s\n", time1);
    printf("\t%s\n", time2); 


    // STACK OVERFLOW 
    time_t t1, t2;
    struct tm tm1 = { 0 };
    struct tm tm2 = { 0 };

    if (strptime(time1, "%a, %d %b %Y %X GMT\r\n\r\n", &tm1) == NULL) {
        printf("fail1\n"); 
    }
    if (strptime(time2, "%a, %d %b %Y %X GMT\r\n\r\n", &tm2) == NULL) {
        printf("fail2\n"); 
    }

    t1 = mktime(&tm1);
    t2 = mktime(&tm2);

    int diff = difftime(t1, t2); 
    printf("->%d<-\n", diff); 
    return diff; 
    // STACK OVERFLOW ENDS

}


int in_cache(struct cacheQueue *q, struct Arguments args, char fileName[], char host[], int connfd) {

    for (int i=0; i < args.cacheSize; i++) {
        if (strcmp(q->arr[i].fileName, fileName) == 0) {

            int proxyfd = create_client_socket(q->arr[i].serverPort);
            char request[100];
            char buf[4096];
            sprintf(request, "HEAD /%s HTTP/1.1\r\nHost: %s\r\n\r\n", fileName, host);
            send(proxyfd, request, strlen(request), 0); 
            recv(proxyfd, buf, 4096, 0);
            close(proxyfd);

            char *ptr = strstr(buf, "Last-Modified: ");
            char *time = strdup(ptr+15);  
            time[strlen(time)-4] = '\0';

            if (compare_times(time, q->arr[i].lastModified) > 0) {
                printf("OUR NEW FILE IS NEWER\n");
                return 0; 
            } else {
                printf("FILE IS CACHED\n");
                int length = q->arr[i].bodySize;
                char response[4096];
                sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nLast-Modified: %s\r\n\r\n", length, time);
                send(connfd, response, strlen(response), 0); 
                send(connfd, q->arr[i].body, q->arr[i].bodySize, 0);
                close(connfd);
                return 1;
            }
        }
    }
    return 0;
}








void cache_file(struct cacheQueue *q, struct Arguments args, int port, char fileName[], char body[], int bodySize, char time[]) {
    if (bodySize > args.cacheSpace) {
        printf("file too large to be cached\n"); 
        return;
    }
        
    // Create and populate File struct to enqueue
    struct File file;
    memcpy(file.fileName, fileName, 20);
    memcpy(file.body, body, 1000);
    memcpy(file.lastModified, time, 100);
    file.bodySize = bodySize;
    file.serverPort = port; 
    
    enqueue_cache(q, file);
    print_cacheQueue(q, args);      
}
