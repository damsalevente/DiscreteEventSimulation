#include "unity.h"
#include "des/des.h"
#include <string.h>

static void makeSimpleConfig(DesSimConfig *cfg) {
    DesConfig_addResource(cfg, "Server", 2);

    int stage = DesConfig_addStage(cfg, "Process");
    DesStage_setResource(cfg, stage, 0);
    int IDLE = DesStage_addState(cfg, stage, "IDLE");
    int BUSY = DesStage_addState(cfg, stage, "BUSY");
    int ENTER = DesStage_addEventType(cfg, stage, "ENTER");
    int DONE = DesStage_addEventType(cfg, stage, "DONE");
    DesStage_setProcessingTime(cfg, stage, DES_DIST_FIXED, 5, 0);
    DesStage_addTransitionIdx(cfg, stage, IDLE, ENTER, BUSY, DES_ACTION_ACQUIRE_AND_PROCESS);
    DesStage_addTransitionIdx(cfg, stage, BUSY, DONE, IDLE, DES_ACTION_RELEASE_AND_DISPATCH);
    DesStage_addOutcomeIdx(cfg, stage, 1.0, DES_INVALID_ID, 0, "FINISH");

    DesConfig_addArrivalIdx(cfg, "Job", 10, stage, DES_DIST_FIXED, 10, 0);
    DesConfig_setSeed(cfg, 42);
    DesConfig_setMaxTime(cfg, 10000);
}

void setUp(void) {}
void tearDown(void) {}

void test_engine_create_destroy(void) {
    DesSimConfig cfg = DesConfig_init();
    makeSimpleConfig(&cfg);
    DesEngine *engine = DesEngine_create(&cfg);
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQUAL_INT(0, engine->current_time);
    TEST_ASSERT_EQUAL_INT(1, engine->num_resource_types);
    TEST_ASSERT_EQUAL_INT(2, engine->resource_types[0].count);
    DesEngine_destroy(engine);
}

void test_engine_run_completes(void) {
    DesSimConfig cfg = DesConfig_init();
    makeSimpleConfig(&cfg);
    DesEngine *engine = DesEngine_create(&cfg);
    DesErrorCode ec = DesEngine_run(engine);
    TEST_ASSERT_EQUAL_INT(DES_OK, ec);
    TEST_ASSERT_EQUAL_INT(10, engine->num_completed_entities);
    DesEngine_destroy(engine);
}

void test_engine_step(void) {
    DesSimConfig cfg = DesConfig_init();
    makeSimpleConfig(&cfg);
    DesEngine *engine = DesEngine_create(&cfg);

    DesErrorCode ec = DesEngine_run(engine);
    TEST_ASSERT_EQUAL_INT(DES_OK, ec);
    TEST_ASSERT_EQUAL_INT(10, engine->num_completed_entities);
    TEST_ASSERT_EQUAL_INT(0, engine->num_active_entities);

    DesEngine_destroy(engine);
}

void test_engine_resource_tracking(void) {
    DesSimConfig cfg = DesConfig_init();
    makeSimpleConfig(&cfg);
    DesEngine *engine = DesEngine_create(&cfg);

    TEST_ASSERT_EQUAL_INT(2, DesResource_getAvailable(engine, 0));

    DesEngine_destroy(engine);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_engine_create_destroy);
    RUN_TEST(test_engine_run_completes);
    RUN_TEST(test_engine_step);
    RUN_TEST(test_engine_resource_tracking);
    return UNITY_END();
}
