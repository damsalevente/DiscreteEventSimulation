#ifndef DES_EVENT_QUEUE_H
#define DES_EVENT_QUEUE_H

#include "des_types.h"

void           DesEventQueue_init(DesEventQueue *q, int capacity);
void           DesEventQueue_destroy(DesEventQueue *q);
void           DesEventQueue_reset(DesEventQueue *q);
bool           DesEventQueue_isEmpty(const DesEventQueue *q);
bool           DesEventQueue_isFull(const DesEventQueue *q);
int            DesEventQueue_size(const DesEventQueue *q);
DesErrorCode   DesEventQueue_enqueue(DesEventQueue *q, const DesEvent *event);
DesErrorCode   DesEventQueue_dequeue(DesEventQueue *q, DesEvent *out);
const DesEvent *DesEventQueue_peek(const DesEventQueue *q);

#endif
