#include "queue.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_QUEUE_SIZE 4096
struct Queue {
    size_t head;
    size_t tail;
    size_t size;
    uint32_t *items;
};

Queue *queue_create() { // initialize queue using code from Dr. Long
    Queue *q = (Queue *) malloc(sizeof(Queue));
    if (q) {
        q->head = 0;
        q->tail = -1;
        q->size = 0;
        q->items = (uint32_t *) calloc(MAX_QUEUE_SIZE, sizeof(uint32_t));
        if (!q->items) {
            free(q);
            q = NULL;
        }
    }
    return q;
}

void queue_delete(Queue **q) { // delete the queue
    if (*q && (*q)->items) {
        free((*q)->items); // free the array too
        free(*q);
        *q = NULL; // setting the pointer to null so no one calls it
    }
    return;
}

bool queue_empty(Queue *q) { // check if top is 0
    return q->size == 0;
}

bool queue_full(Queue *q) { // check if top is equal to capacity
    return q->size == MAX_QUEUE_SIZE;
}

size_t queue_size(Queue *q) {
    return q->size;
}

bool enqueue(Queue *q, uint32_t x) { // push x into the queue
    if (!queue_full(q)) {
        q->tail = (q->tail + 1) % MAX_QUEUE_SIZE;
        q->items[q->tail] = x;
        q->size++;
        return true;
    }
    return false;
}

bool dequeue(Queue *q, uint32_t *x) { // pop form the queue
    if (!queue_empty(q)) {
        *x = q->items[q->head];
        q->head = (q->head + 1) % MAX_QUEUE_SIZE;
        q->size--;
        return true;
    }
    return false;
}

// bool queue_peek(Queue *q, uint32_t *x) {
//     if (!queue_empty(q)) {
//         *x = q->items[q->tail - 1]; // peek without decrementing top
//         return true;
//     }
//     return false;
// }

// void queue_copy(Queue *dst, Queue *src) {
//     free(dst->items); // free the array first
//     dst->items = (uint32_t *) calloc(
//         src->capacity, sizeof(uint32_t)); // allocate space if size is different
//     for (uint32_t i = 0; i < src->top; i++) {
//         dst->items[i] = src->items[i];
//     }
//     dst->top = src->top; // setting everything to the src
//     dst->capacity = src->capacity;
// }
