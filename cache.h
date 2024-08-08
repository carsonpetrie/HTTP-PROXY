#ifndef __CACHE_H__
#define __CACHE_H__

#include "httpproxy.h"


// cacheQueue Implementation --- Circular Array with each index being a File Struct

typedef struct cacheQueue cacheQueue;
cacheQueue *initialize_cacheQueue(int n, File arr[]);
int enqueue_cache (cacheQueue* q, File file);
void dequeue_cache (cacheQueue *q);
void print_cacheQueue(cacheQueue* q, struct Arguments args);


void cache_file(struct cacheQueue *q, struct Arguments args, int port, char fileName[], char body[], int bodySize, char time[]);
int in_cache(struct cacheQueue *q, struct Arguments args, char fileName[], char host[], int connfd);
void insert_cache(struct cacheQueue *q, struct Arguments args, struct File file);

#endif