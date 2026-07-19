#include "des/des_event_queue.h"
#include <stdlib.h>
#include <string.h>

void DesEventQueue_init(DesEventQueue *q, int capacity) {
    if (!q) return;
    if (capacity < 1) capacity = 16;
    q->buffer = (DesEvent *)calloc((size_t)capacity, sizeof(DesEvent));
    q->capacity = capacity;
    q->first = 0;
    q->last = -1;
    q->size = 0;
}

void DesEventQueue_destroy(DesEventQueue *q) {
    free(q->buffer);
    q->buffer = NULL;
    q->capacity = 0;
    q->size = 0;
}

void DesEventQueue_reset(DesEventQueue *q) {
    q->size = 0;
}

bool DesEventQueue_isEmpty(const DesEventQueue *q) {
    return q->size == 0;
}

bool DesEventQueue_isFull(const DesEventQueue *q) {
    return q->size >= q->capacity;
}

int DesEventQueue_size(const DesEventQueue *q) {
    return q->size;
}

static inline bool eventLess(const DesEvent *a, const DesEvent *b) {
    if (a->time != b->time) return a->time < b->time;
    if (a->priority != b->priority) return a->priority < b->priority;
    return a->id < b->id;
}

static void heapSwap(DesEvent *a, DesEvent *b) {
    DesEvent tmp = *a;
    *a = *b;
    *b = tmp;
}

static void heapSiftUp(DesEvent *buf, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (eventLess(&buf[idx], &buf[parent])) {
            heapSwap(&buf[idx], &buf[parent]);
            idx = parent;
        } else {
            break;
        }
    }
}

static void heapSiftDown(DesEvent *buf, int size, int idx) {
    for (;;) {
        int smallest = idx;
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;

        if (left < size && eventLess(&buf[left], &buf[smallest]))
            smallest = left;
        if (right < size && eventLess(&buf[right], &buf[smallest]))
            smallest = right;

        if (smallest != idx) {
            heapSwap(&buf[idx], &buf[smallest]);
            idx = smallest;
        } else {
            break;
        }
    }
}

DesErrorCode DesEventQueue_enqueue(DesEventQueue *q, const DesEvent *event) {
    if (!q || !event) return DES_ERR_NULL_POINTER;
    if (q->size >= q->capacity) {
        int new_capacity = q->capacity < 16 ? 16 : q->capacity * 2;
        DesEvent *new_buffer = (DesEvent *)realloc(q->buffer,
                                                   (size_t)new_capacity * sizeof(DesEvent));
        if (!new_buffer) return DES_ERR_OUT_OF_MEMORY;
        q->buffer = new_buffer;
        q->capacity = new_capacity;
    }
    q->buffer[q->size] = *event;
    heapSiftUp(q->buffer, q->size);
    q->size++;
    return DES_OK;
}

DesErrorCode DesEventQueue_dequeue(DesEventQueue *q, DesEvent *out) {
    if (!q || !out) return DES_ERR_NULL_POINTER;
    if (q->size == 0) return DES_ERR_QUEUE_EMPTY;
    *out = q->buffer[0];
    q->size--;
    if (q->size > 0) {
        q->buffer[0] = q->buffer[q->size];
        heapSiftDown(q->buffer, q->size, 0);
    }
    return DES_OK;
}

const DesEvent *DesEventQueue_peek(const DesEventQueue *q) {
    if (q->size == 0) return NULL;
    return &q->buffer[0];
}
