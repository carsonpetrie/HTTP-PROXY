#ifndef __QUEUE_H__
#define __QUEUE_H__

// Queue.h
// Header file for Queue ADT
// Queue implementation using an array of integers

typedef struct Queue Queue;

// ---------- Constructor Destructor ----------------

Queue *initializeQueue(int n, int *arr);

void destructQueue(Queue *q);

// ----------- Queue Functionality ------------------

int enqueue (Queue* q, int x);

int dequeue (Queue *q);

// ------------ Queue Utility -----------------------

void printQueue(Queue* q);

int count(Queue* q); 

#endif