#ifndef __HTTPPROXY_H__
#define __HTTPPROXY_H__

#include <inttypes.h>

typedef struct Server {
    int ID;
    int port;
    int requests;
    int failures;
    int status;
} Server;

typedef struct Arguments {
    int proxyPort;
    int connections;
    int healthchecks;
    int cacheSpace;
    int cacheSize;
    int caching; 
} Arguments;

typedef struct File {
    char fileName[20];
    char *body;
    char lastModified[100];
    int bodySize;
    int serverPort; 
} File;

int create_client_socket(uint16_t port);
int proxy_request(int connfd, struct Server servers[], int optimalServerIndex);
void healthcheck_servers(struct Server servers[]);
int optimal_server(struct Server servers[]);

#endif