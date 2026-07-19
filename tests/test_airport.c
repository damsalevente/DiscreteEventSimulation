#include "test.h"
#include "des/des.h"
#include <string.h>

static const char *AIRPORT_JSON =
    "{"
    "  \"simulation\": {"
    "    \"max_time\": 500000,"
    "    \"max_events\": 200000,"
    "    \"seed\": 42,"
    "    \"entity_capacity\": 2000"
    "  },"
    "  \"resources\": ["
    "    { \"name\": \"ID_Check_Desk\",   \"count\": 2 },"
    "    { \"name\": \"Security_Scanner\", \"count\": 1 },"
    "    { \"name\": \"Bag_Claim\",       \"count\": 3 }"
    "  ],"
    "  \"stages\": ["
    "    {"
    "      \"name\": \"ID_Check\","
    "      \"resource\": \"ID_Check_Desk\","
    "      \"mode\": \"resource\","
    "      \"processing_time\": { \"distribution\": \"exponential\", \"param1\": 0.2 },"
    "      \"outcomes\": ["
    "        { \"name\": \"PASS\",   \"probability\": 0.90, \"next_stage\": \"Security_Scanner\", \"next_event\": \"ENTER\" },"
    "        { \"name\": \"RETRY\",  \"probability\": 0.05, \"next_stage\": \"ID_Check\",         \"next_event\": \"ENTER\" },"
    "        { \"name\": \"DENIED\", \"probability\": 0.05, \"next_stage\": null }"
    "      ]"
    "    },"
    "    {"
    "      \"name\": \"Security_Scanner\","
    "      \"resource\": \"Security_Scanner\","
    "      \"mode\": \"resource\","
    "      \"processing_time\": { \"distribution\": \"exponential\", \"param1\": 0.1 },"
    "      \"outcomes\": ["
    "        { \"name\": \"CLEAR\",    \"probability\": 0.85, \"next_stage\": \"Bag_Collection\", \"next_event\": \"ENTER\" },"
    "        { \"name\": \"FLAGGED\",  \"probability\": 0.10, \"next_stage\": \"ID_Check\",       \"next_event\": \"ENTER\" },"
    "        { \"name\": \"INCIDENT\", \"probability\": 0.05, \"next_stage\": null }"
    "      ]"
    "    },"
    "    {"
    "      \"name\": \"Bag_Collection\","
    "      \"resource\": \"Bag_Claim\","
    "      \"mode\": \"resource\","
    "      \"processing_time\": { \"distribution\": \"exponential\", \"param1\": 0.33 },"
    "      \"outcomes\": ["
    "        { \"name\": \"DONE\", \"probability\": 1.0, \"next_stage\": null }"
    "      ]"
    "    }"
    "  ],"
    "  \"entity_arrivals\": ["
    "    {"
    "      \"name\": \"Passenger\","
    "      \"count\": 200,"
    "      \"entry_stage\": \"ID_Check\","
    "      \"inter_arrival\": { \"distribution\": \"exponential\", \"param1\": 0.0167 }"
    "    }"
    "  ],"
    "  \"statistics\": {"
    "    \"record_events\": true,"
    "    \"record_entity_flow\": true,"
    "    \"record_resource_util\": true,"
    "    \"output_dir\": \"./output/airport_test\""
    "  }"
    "}";

void setUp(void) {}
void tearDown(void) {}

void test_airport_config_loads(void) {
    DesSimConfig *cfg = DesConfig_loadJsonString(AIRPORT_JSON);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL_INT(3, cfg->num_resources);
    TEST_ASSERT_EQUAL_INT(3, cfg->num_stages);
    TEST_ASSERT_EQUAL_INT(1, cfg->num_arrivals);
    TEST_ASSERT_EQUAL_INT(200, cfg->arrivals[0].entity_count);
    DesConfig_destroy(cfg);
}

void test_airport_config_resources(void) {
    DesSimConfig *cfg = DesConfig_loadJsonString(AIRPORT_JSON);
    TEST_ASSERT_EQUAL_STRING("ID_Check_Desk", cfg->resources[0].name);
    TEST_ASSERT_EQUAL_INT(2, cfg->resources[0].count);
    TEST_ASSERT_EQUAL_STRING("Security_Scanner", cfg->resources[1].name);
    TEST_ASSERT_EQUAL_INT(1, cfg->resources[1].count);
    TEST_ASSERT_EQUAL_STRING("Bag_Claim", cfg->resources[2].name);
    TEST_ASSERT_EQUAL_INT(3, cfg->resources[2].count);
    DesConfig_destroy(cfg);
}

void test_airport_config_stages(void) {
    DesSimConfig *cfg = DesConfig_loadJsonString(AIRPORT_JSON);
    TEST_ASSERT_EQUAL_STRING("ID_Check", cfg->stages[0].name);
    TEST_ASSERT_EQUAL_INT(2, cfg->stages[0].num_states);
    TEST_ASSERT_EQUAL_INT(2, cfg->stages[0].num_event_types);
    TEST_ASSERT_EQUAL_INT(2, cfg->stages[0].num_transitions);
    TEST_ASSERT_EQUAL_INT(3, cfg->stages[0].num_outcomes);

    TEST_ASSERT_EQUAL_STRING("Security_Scanner", cfg->stages[1].name);
    TEST_ASSERT_EQUAL_INT(3, cfg->stages[1].num_outcomes);

    TEST_ASSERT_EQUAL_STRING("Bag_Collection", cfg->stages[2].name);
    TEST_ASSERT_EQUAL_INT(1, cfg->stages[2].num_outcomes);
    DesConfig_destroy(cfg);
}

void test_airport_config_outcomes(void) {
    DesSimConfig *cfg = DesConfig_loadJsonString(AIRPORT_JSON);

    DesStageOutcome *o = cfg->stages[0].outcomes;
    TEST_ASSERT_EQUAL_STRING("PASS", o[0].name);
    TEST_ASSERT_TRUE(o[0].probability > 0.89 && o[0].probability < 0.91);
    TEST_ASSERT_EQUAL_INT(1, o[0].next_stage_id);

    TEST_ASSERT_EQUAL_STRING("RETRY", o[1].name);
    TEST_ASSERT_EQUAL_INT(0, o[1].next_stage_id);

    TEST_ASSERT_EQUAL_STRING("DENIED", o[2].name);
    TEST_ASSERT_EQUAL_INT(DES_INVALID_ID, o[2].next_stage_id);

    DesConfig_destroy(cfg);
}

void test_airport_simulation_runs(void) {
    DesSimConfig *cfg = DesConfig_loadJsonString(AIRPORT_JSON);
    DesEngine *engine = DesEngine_create(cfg);

    DesErrorCode ec = DesEngine_run(engine);
    TEST_ASSERT_EQUAL_INT(DES_OK, ec);

    int total = DesEngine_getEntityCount(engine);
    TEST_ASSERT_EQUAL_INT(200, total);

    int completed = 0;
    for (int i = 0; i < total; i++) {
        const DesEntity *e = DesEntity_get(engine, i);
        if (e && !e->active) completed++;
    }
    TEST_ASSERT_EQUAL_INT(200, completed);

    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_airport_simulation_outcomes_distributed(void) {
    DesSimConfig *cfg = DesConfig_loadJsonString(AIRPORT_JSON);
    DesEngine *engine = DesEngine_create(cfg);
    DesEngine_run(engine);

    int total = DesEngine_getEntityCount(engine);
    int full_pipeline = 0;
    int exited_early = 0;

    for (int i = 0; i < total; i++) {
        const DesEntity *e = DesEntity_get(engine, i);
        if (e && !e->active) {
            if (e->num_stage_visits >= 3) {
                full_pipeline++;
            } else {
                exited_early++;
            }
        }
    }

    TEST_ASSERT_EQUAL_INT(200, full_pipeline + exited_early);
    TEST_ASSERT_TRUE(full_pipeline > 0);
    TEST_ASSERT_TRUE(exited_early > 0);

    printf("\n  Outcomes: full_pipeline=%d exited_early=%d\n", full_pipeline, exited_early);

    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_airport_simulation_flow_times(void) {
    DesSimConfig *cfg = DesConfig_loadJsonString(AIRPORT_JSON);
    DesEngine *engine = DesEngine_create(cfg);
    DesEngine_run(engine);

    int total = DesEngine_getEntityCount(engine);
    int total_flow = 0;
    int count = 0;

    for (int i = 0; i < total; i++) {
        const DesEntity *e = DesEntity_get(engine, i);
        if (e && e->completion_time > 0) {
            int dur = e->completion_time - e->entry_time;
            total_flow += dur;
            count++;
        }
    }

    TEST_ASSERT_TRUE(count > 0);
    int avg = total_flow / count;
    printf("\n  Avg flow time: %d (over %d completed entities)\n", avg, count);
    TEST_ASSERT_TRUE(avg > 0);

    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_airport_simulation_resources_available_after(void) {
    DesSimConfig *cfg = DesConfig_loadJsonString(AIRPORT_JSON);
    DesEngine *engine = DesEngine_create(cfg);
    DesEngine_run(engine);

    TEST_ASSERT_EQUAL_INT(2, DesResource_getAvailable(engine, 0));
    TEST_ASSERT_EQUAL_INT(1, DesResource_getAvailable(engine, 1));
    TEST_ASSERT_EQUAL_INT(3, DesResource_getAvailable(engine, 2));

    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_airport_simulation_file_config(void) {
    DesSimConfig *cfg = DesConfig_loadJson("configs/airport_security.json");
    TEST_ASSERT_NOT_NULL(cfg);

    DesEngine *engine = DesEngine_create(cfg);
    DesErrorCode ec = DesEngine_run(engine);
    TEST_ASSERT_EQUAL_INT(DES_OK, ec);
    TEST_ASSERT_EQUAL_INT(200, DesEngine_getEntityCount(engine));

    DesStats_generateReport(engine);

    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_airport_config_loads);
    RUN_TEST(test_airport_config_resources);
    RUN_TEST(test_airport_config_stages);
    RUN_TEST(test_airport_config_outcomes);
    RUN_TEST(test_airport_simulation_runs);
    RUN_TEST(test_airport_simulation_outcomes_distributed);
    RUN_TEST(test_airport_simulation_flow_times);
    RUN_TEST(test_airport_simulation_resources_available_after);
    RUN_TEST(test_airport_simulation_file_config);
    return UNITY_END();
}
