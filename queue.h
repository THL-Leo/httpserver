#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Queue Queue;

Queue *queue_create();

void queue_delete(Queue **q);

bool queue_empty(Queue *q);

bool queue_full(Queue *q);

size_t queue_size(Queue *q);

bool enqueue(Queue *q, uint32_t x);

bool dequeue(Queue *q, uint32_t *x);

// bool queue_peek(Queue *q, uint32_t *x);

// void queue_copy(Queue *dst, Queue *qrc);
