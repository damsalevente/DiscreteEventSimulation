#include "test.h"
#include "des/des.h"

void setUp(void) {}
void tearDown(void) {}

void test_queue_init_empty(void) {
    DesEventQueue q;
    DesEventQueue_init(&q, 100);
    TEST_ASSERT_TRUE(DesEventQueue_isEmpty(&q));
    TEST_ASSERT_EQUAL_INT(0, DesEventQueue_size(&q));
    DesEventQueue_destroy(&q);
}

void test_queue_enqueue_dequeue(void) {
    DesEventQueue q;
    DesEventQueue_init(&q, 100);

    DesEvent ev = {0};
    ev.id = 1;
    ev.time = 10;
    ev.entity_id = 0;
    ev.target_stage_id = 0;
    ev.event_type = 0;

    TEST_ASSERT_EQUAL_INT(DES_OK, DesEventQueue_enqueue(&q, &ev));
    TEST_ASSERT_FALSE(DesEventQueue_isEmpty(&q));
    TEST_ASSERT_EQUAL_INT(1, DesEventQueue_size(&q));

    DesEvent out;
    TEST_ASSERT_EQUAL_INT(DES_OK, DesEventQueue_dequeue(&q, &out));
    TEST_ASSERT_EQUAL_INT(1, out.id);
    TEST_ASSERT_EQUAL_INT(10, out.time);
    TEST_ASSERT_TRUE(DesEventQueue_isEmpty(&q));

    DesEventQueue_destroy(&q);
}

void test_queue_full(void) {
    DesEventQueue q;
    DesEventQueue_init(&q, 2);

    DesEvent ev = {0};
    ev.id = 1;
    TEST_ASSERT_EQUAL_INT(DES_OK, DesEventQueue_enqueue(&q, &ev));
    TEST_ASSERT_EQUAL_INT(DES_OK, DesEventQueue_enqueue(&q, &ev));
    TEST_ASSERT_EQUAL_INT(DES_OK, DesEventQueue_enqueue(&q, &ev));
    TEST_ASSERT_EQUAL_INT(3, DesEventQueue_size(&q));
    TEST_ASSERT_TRUE(q.capacity >= 3);

    DesEventQueue_destroy(&q);
}

void test_queue_empty_errors(void) {
    DesEventQueue q;
    DesEventQueue_init(&q, 10);

    DesEvent out;
    TEST_ASSERT_EQUAL_INT(DES_ERR_QUEUE_EMPTY, DesEventQueue_dequeue(&q, &out));
    TEST_ASSERT_NULL(DesEventQueue_peek(&q));

    DesEventQueue_destroy(&q);
}

void test_queue_heap_order(void) {
    DesEventQueue q;
    DesEventQueue_init(&q, 100);

    DesEvent ev;
    ev = (DesEvent){0}; ev.id = 1; ev.time = 30; ev.priority = 0;
    DesEventQueue_enqueue(&q, &ev);
    ev = (DesEvent){0}; ev.id = 2; ev.time = 10; ev.priority = 0;
    DesEventQueue_enqueue(&q, &ev);
    ev = (DesEvent){0}; ev.id = 3; ev.time = 20; ev.priority = 0;
    DesEventQueue_enqueue(&q, &ev);

    DesEvent out;
    DesEventQueue_dequeue(&q, &out);
    TEST_ASSERT_EQUAL_INT(2, out.id);
    DesEventQueue_dequeue(&q, &out);
    TEST_ASSERT_EQUAL_INT(3, out.id);
    DesEventQueue_dequeue(&q, &out);
    TEST_ASSERT_EQUAL_INT(1, out.id);

    DesEventQueue_destroy(&q);
}

void test_queue_reset(void) {
    DesEventQueue q;
    DesEventQueue_init(&q, 10);

    DesEvent ev = {0}; ev.id = 1;
    DesEventQueue_enqueue(&q, &ev);
    DesEventQueue_reset(&q);

    TEST_ASSERT_TRUE(DesEventQueue_isEmpty(&q));
    TEST_ASSERT_EQUAL_INT(0, DesEventQueue_size(&q));

    DesEventQueue_destroy(&q);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_queue_init_empty);
    RUN_TEST(test_queue_enqueue_dequeue);
    RUN_TEST(test_queue_full);
    RUN_TEST(test_queue_empty_errors);
    RUN_TEST(test_queue_heap_order);
    RUN_TEST(test_queue_reset);
    return UNITY_END();
}
