#include "test.h"
#include "des/des.h"
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

static void makeValidConfig(DesSimConfig *cfg) {
    int resource = DesConfig_addResource(cfg, "Team", 2);
    int stage = DesConfig_addStage(cfg, "Build");
    DesStage_setResourceMode(cfg, stage, resource, DES_DIST_FIXED, 5, 0);
    DesStage_addOutcomeIdx(cfg, stage, 1.0, DES_INVALID_ID, 0, "DONE");
    DesConfig_addArrivalIdx(cfg, "Release", 3, stage, DES_DIST_FIXED, 10, 0);
}

void test_default_initializer_is_runnable(void) {
    DesSimConfig *cfg = DesConfig_create();
    TEST_ASSERT_EQUAL_INT(100000, cfg->max_time);
    TEST_ASSERT_EQUAL_INT(DES_MAX_EVENTS, cfg->max_events);
    TEST_ASSERT_TRUE(cfg->stats.record_entity_flow);
    DesConfig_destroy(cfg);
}

void test_valid_configuration_passes(void) {
    DesSimConfig *cfg = DesConfig_create();
    makeValidConfig(cfg);
    DesValidationResult result;
    TEST_ASSERT_TRUE(DesConfig_validate(cfg, &result));
    TEST_ASSERT_EQUAL_INT(0, result.num_errors);
    DesConfig_destroy(cfg);
}

void test_probability_sum_is_rejected(void) {
    DesSimConfig *cfg = DesConfig_create();
    makeValidConfig(cfg);
    cfg->stages[0].outcomes[0].probability = 0.8;
    DesValidationResult result;
    TEST_ASSERT_FALSE(DesConfig_validate(cfg, &result));
    TEST_ASSERT_TRUE(result.num_errors > 0);
    DesConfig_destroy(cfg);
}

void test_invalid_json_model_is_rejected(void) {
    const char *json =
        "{\"simulation\":{\"max_time\":100,\"max_events\":100},"
        "\"resources\":[{\"name\":\"Team\",\"count\":1}],"
        "\"stages\":[{\"name\":\"Build\",\"mode\":\"resource\",\"resource\":\"Team\","
        "\"processing_time\":{\"distribution\":\"fixed\",\"param1\":5},"
        "\"outcomes\":[{\"name\":\"A\",\"probability\":0.4},{\"name\":\"B\",\"probability\":0.4}]}],"
        "\"entity_arrivals\":[{\"name\":\"Release\",\"count\":1,\"entry_stage\":\"Build\","
        "\"inter_arrival\":{\"distribution\":\"fixed\",\"param1\":1}}]}";
    DesSimConfig *cfg = DesConfig_loadJsonString(json);
    TEST_ASSERT_NULL(cfg);
    TEST_ASSERT_TRUE(DesConfig_getLoadError()[0] != '\0');
}

void test_json_round_trip_preserves_distributions(void) {
    const char *path = "/tmp/des_validation_roundtrip.json";
    DesSimConfig *cfg = DesConfig_create();
    makeValidConfig(cfg);
    cfg->arrivals[0].inter_arrival.type = DES_DIST_EXPONENTIAL;
    cfg->arrivals[0].inter_arrival.param1 = 0.25;
    TEST_ASSERT_EQUAL_INT(DES_OK, DesConfig_saveJson(cfg, path));
    DesSimConfig *loaded = DesConfig_loadJson(path);
    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_EQUAL_INT(DES_DIST_EXPONENTIAL, loaded->arrivals[0].inter_arrival.type);
    TEST_ASSERT_TRUE(loaded->arrivals[0].inter_arrival.param1 > 0.249);
    TEST_ASSERT_EQUAL_INT(2, loaded->resources[0].count);
    DesConfig_destroy(loaded);
    DesConfig_destroy(cfg);
    remove(path);
}

void test_json_round_trip_preserves_initial_state(void) {
    const char *path = "/tmp/des_initial_state_roundtrip.json";
    DesSimConfig *cfg = DesConfig_create();
    int stage = DesConfig_addStage(cfg, "Manual");
    DesStage_addState(cfg, stage, "COLD");
    int ready = DesStage_addState(cfg, stage, "READY");
    DesStage_addEventType(cfg, stage, "ENTER");
    DesStage_setInitialState(cfg, stage, ready);
    DesStage_setProcessingTime(cfg, stage, DES_DIST_FIXED, 1, 0);
    DesStage_addTransitionIdx(cfg, stage, ready, 0, ready, DES_ACTION_ENTITY_EXIT);
    DesConfig_addArrivalIdx(cfg, "Entity", 1, stage, DES_DIST_FIXED, 1, 0);
    TEST_ASSERT_EQUAL_INT(DES_OK, DesConfig_saveJson(cfg, path));
    DesSimConfig *loaded = DesConfig_loadJson(path);
    TEST_ASSERT_NOT_NULL(loaded);
    TEST_ASSERT_EQUAL_INT(ready, loaded->stages[0].initial_state_index);
    TEST_ASSERT_EQUAL_STRING("READY", loaded->stages[0].state_names[loaded->stages[0].initial_state_index]);
    DesConfig_destroy(loaded);
    DesConfig_destroy(cfg);
    remove(path);
}

void test_invalid_initial_state_is_rejected(void) {
    DesSimConfig *cfg = DesConfig_create();
    makeValidConfig(cfg);
    cfg->stages[0].initial_state_index = 8;
    DesValidationResult result;
    TEST_ASSERT_FALSE(DesConfig_validate(cfg, &result));
    DesConfig_destroy(cfg);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_default_initializer_is_runnable);
    RUN_TEST(test_valid_configuration_passes);
    RUN_TEST(test_probability_sum_is_rejected);
    RUN_TEST(test_invalid_json_model_is_rejected);
    RUN_TEST(test_json_round_trip_preserves_distributions);
    RUN_TEST(test_json_round_trip_preserves_initial_state);
    RUN_TEST(test_invalid_initial_state_is_rejected);
    return UNITY_END();
}
