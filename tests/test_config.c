#include "test.h"
#include "des/des.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_config_create_defaults(void) {
    DesSimConfig *cfg = DesConfig_create();
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL_INT(0, cfg->num_resources);
    TEST_ASSERT_EQUAL_INT(0, cfg->num_stages);
    TEST_ASSERT_EQUAL_INT(0, cfg->num_arrivals);
    TEST_ASSERT_EQUAL_INT(100000, cfg->max_time);
    DesConfig_destroy(cfg);
}

void test_config_stack_init(void) {
    DesSimConfig *cfg = DesConfig_create();
    TEST_ASSERT_EQUAL_INT(0, cfg->num_resources);
    TEST_ASSERT_EQUAL_INT(0, cfg->num_stages);
    TEST_ASSERT_EQUAL_INT(0, cfg->num_arrivals);
    TEST_ASSERT_EQUAL_INT(100000, cfg->max_time);
    DesConfig_destroy(cfg);
}

void test_config_add_resource(void) {
    DesSimConfig *cfg = DesConfig_create();
    int id = DesConfig_addResource(cfg, "HIL", 3);
    TEST_ASSERT_EQUAL_INT(0, id);
    TEST_ASSERT_EQUAL_INT(1, cfg->num_resources);
    TEST_ASSERT_EQUAL_STRING("HIL", cfg->resources[0].name);
    TEST_ASSERT_EQUAL_INT(3, cfg->resources[0].count);
    DesConfig_destroy(cfg);
}

void test_config_add_stage(void) {
    DesSimConfig *cfg = DesConfig_create();
    int id = DesConfig_addStage(cfg, "TestStage");
    TEST_ASSERT_EQUAL_INT(0, id);
    TEST_ASSERT_EQUAL_INT(1, cfg->num_stages);
    TEST_ASSERT_EQUAL_STRING("TestStage", cfg->stages[0].name);

    DesStage_addState(cfg, id, "IDLE");
    DesStage_addState(cfg, id, "BUSY");
    TEST_ASSERT_EQUAL_INT(2, cfg->stages[0].num_states);

    DesStage_addEventType(cfg, id, "ENTER");
    TEST_ASSERT_EQUAL_INT(1, cfg->stages[0].num_event_types);

    DesConfig_destroy(cfg);
}

void test_config_add_transition_idx(void) {
    DesSimConfig *cfg = DesConfig_create();
    int sid = DesConfig_addStage(cfg, "S1");
    int IDLE = DesStage_addState(cfg, sid, "IDLE");
    int BUSY = DesStage_addState(cfg, sid, "BUSY");
    int E1 = DesStage_addEventType(cfg, sid, "E1");
    DesStage_addTransitionIdx(cfg, sid, IDLE, E1, BUSY, DES_ACTION_ACQUIRE_AND_PROCESS);

    TEST_ASSERT_EQUAL_INT(1, cfg->stages[sid].num_transitions);
    TEST_ASSERT_EQUAL_INT(IDLE, cfg->stages[sid].transitions[0].state_index);
    TEST_ASSERT_EQUAL_INT(E1, cfg->stages[sid].transitions[0].event_index);
    TEST_ASSERT_EQUAL_INT(BUSY, cfg->stages[sid].transitions[0].next_state_index);
    TEST_ASSERT_EQUAL_INT(DES_ACTION_ACQUIRE_AND_PROCESS,
                          cfg->stages[sid].transitions[0].action_type);
    DesConfig_destroy(cfg);
}

void test_config_add_transition_string(void) {
    DesSimConfig *cfg = DesConfig_create();
    int sid = DesConfig_addStage(cfg, "S1");
    DesStage_addState(cfg, sid, "IDLE");
    DesStage_addState(cfg, sid, "BUSY");
    DesStage_addEventType(cfg, sid, "E1");
    DesStage_addTransition(cfg, sid, "IDLE", "E1", "BUSY", DES_ACTION_ACQUIRE_AND_PROCESS);

    TEST_ASSERT_EQUAL_INT(1, cfg->stages[sid].num_transitions);
    TEST_ASSERT_EQUAL_INT(0, cfg->stages[sid].transitions[0].state_index);
    TEST_ASSERT_EQUAL_INT(0, cfg->stages[sid].transitions[0].event_index);
    TEST_ASSERT_EQUAL_INT(1, cfg->stages[sid].transitions[0].next_state_index);
    TEST_ASSERT_EQUAL_INT(DES_ACTION_ACQUIRE_AND_PROCESS,
                          cfg->stages[sid].transitions[0].action_type);

    DesConfig_destroy(cfg);
}

void test_config_add_outcome_idx(void) {
    DesSimConfig *cfg = DesConfig_create();
    int s1 = DesConfig_addStage(cfg, "Stage1");
    int s2 = DesConfig_addStage(cfg, "Stage2");
    int ENTER = DesStage_addEventType(cfg, s2, "ENTER");
    DesStage_addOutcomeIdx(cfg, s1, 0.7, s2, ENTER, "PASS");
    DesStage_addOutcomeIdx(cfg, s1, 0.3, DES_INVALID_ID, 0, "FAIL");

    TEST_ASSERT_EQUAL_INT(2, cfg->stages[s1].num_outcomes);
    TEST_ASSERT_EQUAL_INT(s2, cfg->stages[s1].outcomes[0].next_stage_id);
    TEST_ASSERT_TRUE(cfg->stages[s1].outcomes[0].probability > 0.69);
    TEST_ASSERT_TRUE(cfg->stages[s1].outcomes[0].probability < 0.71);
    TEST_ASSERT_EQUAL_INT(DES_INVALID_ID, cfg->stages[s1].outcomes[1].next_stage_id);
    DesConfig_destroy(cfg);
}

void test_config_add_outcome_string(void) {
    DesSimConfig *cfg = DesConfig_create();
    int s1 = DesConfig_addStage(cfg, "Stage1");
    int s2 = DesConfig_addStage(cfg, "Stage2");
    DesStage_addEventType(cfg, s2, "ENTER");
    DesStage_addOutcome(cfg, s1, 0.7, "Stage2", "ENTER", "PASS");
    DesStage_addOutcome(cfg, s1, 0.3, NULL, NULL, "FAIL");

    TEST_ASSERT_EQUAL_INT(2, cfg->stages[s1].num_outcomes);
    TEST_ASSERT_EQUAL_INT(s2, cfg->stages[s1].outcomes[0].next_stage_id);
    TEST_ASSERT_TRUE(cfg->stages[s1].outcomes[0].probability > 0.69);
    TEST_ASSERT_TRUE(cfg->stages[s1].outcomes[0].probability < 0.71);
    TEST_ASSERT_EQUAL_INT(DES_INVALID_ID, cfg->stages[s1].outcomes[1].next_stage_id);

    DesConfig_destroy(cfg);
}

void test_config_add_arrival_idx(void) {
    DesSimConfig *cfg = DesConfig_create();
    int s = DesConfig_addStage(cfg, "Entry");
    int a = DesConfig_addArrivalIdx(cfg, "Job", 10, s, DES_DIST_FIXED, 5, 0);
    TEST_ASSERT_EQUAL_INT(0, a);
    TEST_ASSERT_EQUAL_INT(1, cfg->num_arrivals);
    TEST_ASSERT_EQUAL_STRING("Job", cfg->arrivals[0].name);
    TEST_ASSERT_EQUAL_INT(10, cfg->arrivals[0].entity_count);
    TEST_ASSERT_EQUAL_INT(s, cfg->arrivals[0].entry_stage_id);
    DesConfig_destroy(cfg);
}

void test_json_load_string(void) {
    const char *json =
        "{"
        "  \"simulation\": { \"max_time\": 5000, \"seed\": 42 },"
        "  \"resources\": [ { \"name\": \"R1\", \"count\": 2 } ],"
        "  \"stages\": [ {"
        "    \"name\": \"S1\","
        "    \"resource\": \"R1\","
        "    \"states\": [\"IDLE\", \"BUSY\"],"
        "    \"event_types\": [\"ENTER\", \"DONE\"],"
        "    \"processing_time\": { \"distribution\": \"fixed\", \"param1\": 5 },"
        "    \"fsm\": ["
        "      { \"state\": \"IDLE\", \"event\": \"ENTER\", \"next_state\": \"BUSY\", \"action\": \"acquire_and_process\" },"
        "      { \"state\": \"BUSY\", \"event\": \"DONE\", \"next_state\": \"IDLE\", \"action\": \"release_and_dispatch\" }"
        "    ],"
        "    \"outcomes\": ["
        "      { \"name\": \"OK\", \"probability\": 1.0, \"next_stage\": null }"
        "    ]"
        "  } ],"
        "  \"entity_arrivals\": [ {"
        "    \"name\": \"Job\", \"count\": 5, \"entry_stage\": \"S1\","
        "    \"inter_arrival\": { \"distribution\": \"fixed\", \"param1\": 10 }"
        "  } ]"
        "}";

    DesSimConfig *cfg = DesConfig_loadJsonString(json);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL_INT(5000, cfg->max_time);
    TEST_ASSERT_EQUAL_INT(42, (int)cfg->seed);
    TEST_ASSERT_EQUAL_INT(1, cfg->num_resources);
    TEST_ASSERT_EQUAL_STRING("R1", cfg->resources[0].name);
    TEST_ASSERT_EQUAL_INT(2, cfg->resources[0].count);
    TEST_ASSERT_EQUAL_INT(1, cfg->num_stages);
    TEST_ASSERT_EQUAL_STRING("S1", cfg->stages[0].name);
    TEST_ASSERT_EQUAL_INT(2, cfg->stages[0].num_states);
    TEST_ASSERT_EQUAL_INT(2, cfg->stages[0].num_event_types);
    TEST_ASSERT_EQUAL_INT(2, cfg->stages[0].num_transitions);
    TEST_ASSERT_EQUAL_INT(1, cfg->stages[0].num_outcomes);
    TEST_ASSERT_EQUAL_INT(1, cfg->num_arrivals);
    TEST_ASSERT_EQUAL_INT(5, cfg->arrivals[0].entity_count);
    DesConfig_destroy(cfg);
}

void test_dist_macros(void) {
    DesDistribution d = {0};
    DES_DIST_FMT(5, d);
    TEST_ASSERT_EQUAL_INT(DES_DIST_FIXED, d.type);
    TEST_ASSERT_TRUE(d.param1 == 5.0);

    DES_DIST_EXP(0.5, d);
    TEST_ASSERT_EQUAL_INT(DES_DIST_EXPONENTIAL, d.type);
    TEST_ASSERT_TRUE(d.param1 == 0.5);

    DES_DIST_UNI(1, 10, d);
    TEST_ASSERT_EQUAL_INT(DES_DIST_UNIFORM, d.type);

    DES_DIST_NRM(100.0, 15.0, d);
    TEST_ASSERT_EQUAL_INT(DES_DIST_NORMAL, d.type);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_config_create_defaults);
    RUN_TEST(test_config_stack_init);
    RUN_TEST(test_config_add_resource);
    RUN_TEST(test_config_add_stage);
    RUN_TEST(test_config_add_transition_idx);
    RUN_TEST(test_config_add_transition_string);
    RUN_TEST(test_config_add_outcome_idx);
    RUN_TEST(test_config_add_outcome_string);
    RUN_TEST(test_config_add_arrival_idx);
    RUN_TEST(test_json_load_string);
    RUN_TEST(test_dist_macros);
    return UNITY_END();
}
