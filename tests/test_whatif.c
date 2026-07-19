#include "test.h"
#include "des/des.h"
#include <string.h>

static void makeWhatIfConfig(DesSimConfig *cfg) {
    DesConfig_setMaxTime(cfg, 60000);
    DesConfig_setMaxEvents(cfg, 100000);
    DesConfig_setSeed(cfg, 42);

    DesConfig_addResource(cfg, "HIL_Machine", 2);
    DesConfig_setResourceAvailableAt(cfg, 0, 0);
    DesConfig_addResource(cfg, "Vehicle_Bench", 1);
    DesConfig_setResourceAvailableAt(cfg, 1, 0);
    DesConfig_addResource(cfg, "Vehicle", 1);
    DesConfig_setResourceAvailableAt(cfg, 2, 40320);

    /* HIL_Testing: resource=0, states=[IDLE,BUSY], events=[ENTER,COMPLETE] */
    int hil = DesConfig_addStage(cfg, "HIL_Testing");
    DesStage_setResource(cfg, hil, 0);
    int HIL_IDLE = DesStage_addState(cfg, hil, "IDLE");
    int HIL_BUSY = DesStage_addState(cfg, hil, "BUSY");
    int HIL_ENTER = DesStage_addEventType(cfg, hil, "ENTER");
    int HIL_COMPLETE = DesStage_addEventType(cfg, hil, "COMPLETE");
    DesStage_setProcessingTime(cfg, hil, DES_DIST_FIXED, 100, 0);
    DesStage_addTransitionIdx(cfg, hil, HIL_IDLE, HIL_ENTER, HIL_BUSY, DES_ACTION_ACQUIRE_AND_PROCESS);
    DesStage_addTransitionIdx(cfg, hil, HIL_BUSY, HIL_COMPLETE, HIL_IDLE, DES_ACTION_RELEASE_AND_DISPATCH);
    DesStage_addOutcomeIdx(cfg, hil, 0.70, 1, 0, "PASS");   /* -> Bench_Test.ENTER */
    DesStage_addOutcomeIdx(cfg, hil, 0.30, DES_INVALID_ID, 0, "FAIL");

    /* Bench_Test: resource=1 */
    int bench = DesConfig_addStage(cfg, "Bench_Test");
    DesStage_setResource(cfg, bench, 1);
    int BENCH_IDLE = DesStage_addState(cfg, bench, "IDLE");
    int BENCH_BUSY = DesStage_addState(cfg, bench, "BUSY");
    int BENCH_ENTER = DesStage_addEventType(cfg, bench, "ENTER");
    int BENCH_COMPLETE = DesStage_addEventType(cfg, bench, "COMPLETE");
    DesStage_setProcessingTime(cfg, bench, DES_DIST_FIXED, 80, 0);
    DesStage_addTransitionIdx(cfg, bench, BENCH_IDLE, BENCH_ENTER, BENCH_BUSY, DES_ACTION_ACQUIRE_AND_PROCESS);
    DesStage_addTransitionIdx(cfg, bench, BENCH_BUSY, BENCH_COMPLETE, BENCH_IDLE, DES_ACTION_RELEASE_AND_DISPATCH);
    DesStage_addOutcomeIdx(cfg, bench, 0.90, 2, 0, "PASS");  /* -> Vehicle_Test.ENTER */
    DesStage_addOutcomeIdx(cfg, bench, 0.10, 0, 0, "FAIL");  /* -> HIL_Testing.ENTER */

    /* Vehicle_Test: resource=2 */
    int veh = DesConfig_addStage(cfg, "Vehicle_Test");
    DesStage_setResource(cfg, veh, 2);
    int VEH_IDLE = DesStage_addState(cfg, veh, "IDLE");
    int VEH_BUSY = DesStage_addState(cfg, veh, "BUSY");
    int VEH_ENTER = DesStage_addEventType(cfg, veh, "ENTER");
    int VEH_COMPLETE = DesStage_addEventType(cfg, veh, "COMPLETE");
    DesStage_setProcessingTime(cfg, veh, DES_DIST_FIXED, 50, 0);
    DesStage_addTransitionIdx(cfg, veh, VEH_IDLE, VEH_ENTER, VEH_BUSY, DES_ACTION_ACQUIRE_AND_PROCESS);
    DesStage_addTransitionIdx(cfg, veh, VEH_BUSY, VEH_COMPLETE, VEH_IDLE, DES_ACTION_RELEASE_AND_DISPATCH);
    DesStage_addOutcomeIdx(cfg, veh, 0.95, DES_INVALID_ID, 0, "PASS");
    DesStage_addOutcomeIdx(cfg, veh, 0.05, 0, 0, "FAIL");    /* -> HIL_Testing.ENTER */

    int a;
    a = DesConfig_addArrivalIdx(cfg, "Release65", 5, 0, DES_DIST_FIXED, 10, 0);
    DesConfig_setArrivalStart(cfg, a, 0);
    DesConfig_setArrivalPriority(cfg, a, 0);
    a = DesConfig_addArrivalIdx(cfg, "Release66", 5, 0, DES_DIST_FIXED, 10, 0);
    DesConfig_setArrivalStart(cfg, a, 10080);
    DesConfig_setArrivalPriority(cfg, a, 1);
    a = DesConfig_addArrivalIdx(cfg, "Release63", 5, 0, DES_DIST_FIXED, 10, 0);
    DesConfig_setArrivalStart(cfg, a, 20160);
    DesConfig_setArrivalPriority(cfg, a, 2);

    DesConfig_setStats(cfg, true, true, true, "./output/whatif_test");
}

void setUp(void) {}
void tearDown(void) {}

void test_whatif_config_loads(void) {
    DesSimConfig *cfg = DesConfig_loadJson("configs/whatif_release.json");
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL_INT(3, cfg->num_resources);
    TEST_ASSERT_EQUAL_INT(3, cfg->num_stages);
    TEST_ASSERT_EQUAL_INT(3, cfg->num_arrivals);
    DesConfig_destroy(cfg);
}

void test_whatif_config_available_at(void) {
    DesSimConfig *cfg = DesConfig_loadJson("configs/whatif_release.json");
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL_INT(0, cfg->resources[0].available_at);
    TEST_ASSERT_EQUAL_INT(0, cfg->resources[1].available_at);
    TEST_ASSERT_EQUAL_INT(40320, cfg->resources[2].available_at);
    DesConfig_destroy(cfg);
}

void test_whatif_config_arrival_times(void) {
    DesSimConfig *cfg = DesConfig_loadJson("configs/whatif_release.json");
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL_INT(0, cfg->arrivals[0].start_time);
    TEST_ASSERT_EQUAL_INT(10080, cfg->arrivals[1].start_time);
    TEST_ASSERT_EQUAL_INT(20160, cfg->arrivals[2].start_time);
    TEST_ASSERT_EQUAL_INT(0, cfg->arrivals[0].priority);
    TEST_ASSERT_EQUAL_INT(1, cfg->arrivals[1].priority);
    TEST_ASSERT_EQUAL_INT(2, cfg->arrivals[2].priority);
    DesConfig_destroy(cfg);
}

void test_whatif_programmatic_config_runs(void) {
    DesSimConfig *cfg = DesConfig_create();
    makeWhatIfConfig(cfg);
    DesEngine *engine = DesEngine_create(cfg);
    TEST_ASSERT_NOT_NULL(engine);

    DesErrorCode ec = DesEngine_run(engine);
    TEST_ASSERT_EQUAL_INT(DES_OK, ec);
    TEST_ASSERT_EQUAL_INT(15, DesEngine_getEntityCount(engine));

    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_whatif_all_entities_complete(void) {
    DesSimConfig *cfg = DesConfig_create();
    makeWhatIfConfig(cfg);
    DesEngine *engine = DesEngine_create(cfg);
    DesEngine_run(engine);

    TEST_ASSERT_EQUAL_INT(15, engine->num_completed_entities);
    TEST_ASSERT_EQUAL_INT(0, engine->num_active_entities);

    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_whatif_vehicle_delayed(void) {
    DesSimConfig *cfg = DesConfig_loadJson("configs/whatif_release.json");
    TEST_ASSERT_NOT_NULL(cfg);

    DesEngine *engine = DesEngine_create(cfg);
    DesEngine_run(engine);

    TEST_ASSERT_TRUE(engine->current_time >= 40320);
    TEST_ASSERT_EQUAL_INT(45, engine->num_completed_entities);

    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_whatif_json_simulation_runs(void) {
    DesSimConfig *cfg = DesConfig_loadJson("configs/whatif_release.json");
    TEST_ASSERT_NOT_NULL(cfg);

    DesEngine *engine = DesEngine_create(cfg);
    TEST_ASSERT_NOT_NULL(engine);

    DesErrorCode ec = DesEngine_run(engine);
    TEST_ASSERT_EQUAL_INT(DES_OK, ec);
    TEST_ASSERT_EQUAL_INT(45, DesEngine_getEntityCount(engine));

    DesStats_generateReport(engine);

    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_whatif_resource_available_after(void) {
    DesSimConfig *cfg = DesConfig_create();
    makeWhatIfConfig(cfg);
    DesEngine *engine = DesEngine_create(cfg);
    DesEngine_run(engine);

    TEST_ASSERT_EQUAL_INT(2, DesResource_getAvailable(engine, 0));
    TEST_ASSERT_EQUAL_INT(1, DesResource_getAvailable(engine, 1));
    TEST_ASSERT_EQUAL_INT(1, DesResource_getAvailable(engine, 2));

    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_whatif_clog_creates_queue(void) {
    DesSimConfig *cfg = DesConfig_loadJson("configs/whatif_release.json");
    TEST_ASSERT_NOT_NULL(cfg);

    DesEngine *engine = DesEngine_create(cfg);
    DesEngine_run(engine);

    TEST_ASSERT_TRUE(engine->current_time > 40320);
    TEST_ASSERT_EQUAL_INT(45, engine->num_completed_entities);

    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_whatif_config_loads);
    RUN_TEST(test_whatif_config_available_at);
    RUN_TEST(test_whatif_config_arrival_times);
    RUN_TEST(test_whatif_programmatic_config_runs);
    RUN_TEST(test_whatif_all_entities_complete);
    RUN_TEST(test_whatif_vehicle_delayed);
    RUN_TEST(test_whatif_json_simulation_runs);
    RUN_TEST(test_whatif_resource_available_after);
    RUN_TEST(test_whatif_clog_creates_queue);
    return UNITY_END();
}
