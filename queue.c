#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "queue.h"

typedef struct Queue {
    int head;
    int tail;
    int length;
    int count;
    int *arr;
} Queue;

Queue *initializeQueue(int n, int *arr) {
    struct Queue* q = (Queue *)malloc(sizeof(struct Queue));
    assert(q != NULL);
    q->head = 0;
    q->tail = 0;
    q->length = n;
    q->count = 0;
    q->arr = (int *)malloc(n * sizeof(int));
    return q;
}

void destructQueue(Queue *q) {
    assert(q != NULL);
    free(q->arr);
    free(q);
}

int enqueue(Queue *q, int x) {
    if (q->count >= q->length) {
        return -1;
    }
    q->arr[q->tail] = x;
    q->tail = (q->tail + 1) % q->length;
    q->count++;
    return 0; 
}

// LAB SPECIFIC IMPLEMENTATION:
//      returns connfd if it is in queue
//      returns -1-1 if there is no connfd in queue
int dequeue(Queue *q) {
    int value;
    if (q->count == 0) {
        return -1;
    }
    value = q->arr[q->head];
    q->head = (q->head + 1) % q->length;
    q->count--;
    return value; 
}

void printQueue(Queue *q) {
    printf("Queue:");
    for (int i=q->head; i < q->tail; i++) {
        printf(" %d", q->arr[i]);
    }
    printf("\n"); 
}

int count(Queue *q) {
    return q->count; 
}