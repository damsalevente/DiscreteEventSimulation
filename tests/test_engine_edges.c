#include "test.h"
#include "des/des.h"
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void makeDelayedConfig(DesSimConfig *cfg, int start_time) {
    int resource = DesConfig_addResource(cfg, "Engineer", 1);
    int stage = DesConfig_addStage(cfg, "Develop");
    DesStage_setResourceMode(cfg, stage, resource, DES_DIST_FIXED, 5, 0);
    DesStage_addOutcomeIdx(cfg, stage, 1.0, DES_INVALID_ID, 0, "DONE");
    int arrival = DesConfig_addArrivalIdx(cfg, "Release", 1, stage, DES_DIST_FIXED, 1, 0);
    DesConfig_setArrivalStart(cfg, arrival, start_time);
    DesConfig_setSeed(cfg, 7);
}

void test_delayed_arrival_sets_real_entry_time(void) {
    DesSimConfig *cfg = DesConfig_create();
    makeDelayedConfig(cfg, 100);
    DesEngine *engine = DesEngine_create(cfg);
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQUAL_INT(DES_OK, DesEngine_run(engine));
    const DesEntity *entity = DesEntity_get(engine, 0);
    TEST_ASSERT_NOT_NULL(entity);
    TEST_ASSERT_EQUAL_INT(100, entity->entry_time);
    TEST_ASSERT_EQUAL_INT(105, entity->completion_time);
    TEST_ASSERT_EQUAL_INT(5, entity->completion_time - entity->entry_time);
    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_max_time_does_not_process_future_event(void) {
    DesSimConfig *cfg = DesConfig_create();
    makeDelayedConfig(cfg, 100);
    DesConfig_setMaxTime(cfg, 102);
    DesEngine *engine = DesEngine_create(cfg);
    TEST_ASSERT_EQUAL_INT(DES_OK, DesEngine_run(engine));
    TEST_ASSERT_EQUAL_INT(100, engine->current_time);
    TEST_ASSERT_EQUAL_INT(1, engine->num_active_entities);
    TEST_ASSERT_EQUAL_INT(0, engine->num_completed_entities);
    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_max_events_is_processed_event_limit(void) {
    DesSimConfig *cfg = DesConfig_create();
    makeDelayedConfig(cfg, 0);
    DesConfig_setMaxEvents(cfg, 1);
    DesEngine *engine = DesEngine_create(cfg);
    TEST_ASSERT_EQUAL_INT(DES_OK, DesEngine_run(engine));
    TEST_ASSERT_EQUAL_INT(1, engine->events_processed);
    TEST_ASSERT_EQUAL_INT(1, engine->num_active_entities);
    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_second_run_does_not_seed_duplicate_entities(void) {
    DesSimConfig *cfg = DesConfig_create();
    makeDelayedConfig(cfg, 0);
    DesEngine *engine = DesEngine_create(cfg);
    TEST_ASSERT_EQUAL_INT(DES_OK, DesEngine_run(engine));
    TEST_ASSERT_EQUAL_INT(1, DesEngine_getEntityCount(engine));
    TEST_ASSERT_EQUAL_INT(DES_OK, DesEngine_run(engine));
    TEST_ASSERT_EQUAL_INT(1, DesEngine_getEntityCount(engine));
    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_unconstrained_stage_supports_concurrent_entities(void) {
    DesSimConfig *cfg = DesConfig_create();
    int stage = DesConfig_addStage(cfg, "Notify");
    DesStage_setResourceMode(cfg, stage, DES_INVALID_ID, DES_DIST_FIXED, 5, 0);
    DesStage_addOutcomeIdx(cfg, stage, 1.0, DES_INVALID_ID, 0, "DONE");
    DesConfig_addArrivalIdx(cfg, "Message", 2, stage, DES_DIST_FIXED, 0, 0);
    DesEngine *engine = DesEngine_create(cfg);
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQUAL_INT(DES_OK, DesEngine_run(engine));
    TEST_ASSERT_EQUAL_INT(2, engine->num_completed_entities);
    TEST_ASSERT_EQUAL_INT(5, engine->current_time);
    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_manual_initial_state_and_transition_trace(void) {
    DesSimConfig *cfg = DesConfig_create();
    int resource = DesConfig_addResource(cfg, "Worker", 1);
    int stage = DesConfig_addStage(cfg, "Advanced");
    DesStage_setResource(cfg, stage, resource);
    int idle = DesStage_addState(cfg, stage, "IDLE");
    int ready = DesStage_addState(cfg, stage, "READY");
    int busy = DesStage_addState(cfg, stage, "BUSY");
    int enter = DesStage_addEventType(cfg, stage, "ENTER");
    int complete = DesStage_addEventType(cfg, stage, "COMPLETE");
    DesStage_setInitialState(cfg, stage, ready);
    DesStage_setProcessingTime(cfg, stage, DES_DIST_FIXED, 5, 0);
    DesStage_addTransitionIdx(cfg, stage, ready, enter, busy, DES_ACTION_ACQUIRE_AND_PROCESS);
    DesStage_addTransitionIdx(cfg, stage, busy, complete, ready, DES_ACTION_RELEASE_AND_DISPATCH);
    DesStage_addOutcomeIdx(cfg, stage, 1.0, DES_INVALID_ID, 0, "DONE");
    DesConfig_addArrivalIdx(cfg, "Job", 1, stage, DES_DIST_FIXED, 0, 0);
    cfg->stats.record_events = true;
    cfg->stats.record_entity_flow = true;
    cfg->stats.record_resource_util = true;

    DesEngine *engine = DesEngine_create(cfg);
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQUAL_INT(DES_OK, DesEngine_run(engine));
    TEST_ASSERT_EQUAL_INT(2, engine->stats.num_transition_records);
    TEST_ASSERT_EQUAL_INT(ready, engine->stats.transition_records[0].from_state);
    TEST_ASSERT_EQUAL_INT(busy, engine->stats.transition_records[0].to_state);
    TEST_ASSERT_TRUE(engine->stats.transition_records[0].accepted);
    TEST_ASSERT_EQUAL_INT(0, engine->stats.transition_records[0].resource_instance_id);
    TEST_ASSERT_EQUAL_INT(idle, 0);

    const char *path = "/tmp/des_replay_trace_test.json";
    TEST_ASSERT_EQUAL_INT(DES_OK, DesStats_exportReplayJson(engine, path));
    FILE *file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    char buffer[1024];
    size_t count = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[count] = '\0';
    fclose(file);
    TEST_ASSERT_NOT_NULL(strstr(buffer, "\"replay_version\": 1"));
    remove(path);
    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_resource_contention_records_rejected_attempt(void) {
    DesSimConfig *cfg = DesConfig_create();
    int resource = DesConfig_addResource(cfg, "Worker", 1);
    int stage = DesConfig_addStage(cfg, "Work");
    DesStage_setResourceMode(cfg, stage, resource, DES_DIST_FIXED, 5, 0);
    DesStage_addOutcomeIdx(cfg, stage, 1.0, DES_INVALID_ID, 0, "DONE");
    DesConfig_addArrivalIdx(cfg, "Job", 2, stage, DES_DIST_FIXED, 0, 0);
    cfg->stats.record_events = true;
    DesEngine *engine = DesEngine_create(cfg);
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQUAL_INT(DES_OK, DesEngine_run(engine));
    bool found_rejected = false;
    for (int i = 0; i < engine->stats.num_transition_records; i++) {
        const DesTransitionRecord *record = &engine->stats.transition_records[i];
        if (record->action_type == DES_ACTION_ACQUIRE_AND_PROCESS && !record->accepted)
            found_rejected = true;
    }
    TEST_ASSERT_TRUE(found_rejected);
    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

void test_shared_resource_stages_keep_distinct_entity_state_machines(void) {
    DesSimConfig *cfg = DesConfig_create();
    int resource = DesConfig_addResource(cfg, "Shared", 1);
    int first = DesConfig_addStage(cfg, "First");
    DesStage_setResourceMode(cfg, first, resource, DES_DIST_FIXED, 2, 0);

    int second = DesConfig_addStage(cfg, "Second");
    DesStage_setResource(cfg, second, resource);
    DesStage_addState(cfg, second, "UNUSED");
    int ready = DesStage_addState(cfg, second, "READY");
    int busy = DesStage_addState(cfg, second, "ACTIVE");
    int enter = DesStage_addEventType(cfg, second, "START");
    int complete = DesStage_addEventType(cfg, second, "FINISH");
    DesStage_setInitialState(cfg, second, ready);
    DesStage_setProcessingTime(cfg, second, DES_DIST_FIXED, 3, 0);
    DesStage_addTransitionIdx(cfg, second, ready, enter, busy, DES_ACTION_ACQUIRE_AND_PROCESS);
    DesStage_addTransitionIdx(cfg, second, busy, complete, ready, DES_ACTION_RELEASE_AND_DISPATCH);

    DesStage_addOutcomeIdx(cfg, first, 1.0, second, enter, "NEXT");
    DesStage_addOutcomeIdx(cfg, second, 1.0, DES_INVALID_ID, 0, "DONE");
    DesConfig_addArrivalIdx(cfg, "Job", 1, first, DES_DIST_FIXED, 0, 0);
    cfg->stats.record_events = true;

    DesEngine *engine = DesEngine_create(cfg);
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQUAL_INT(DES_OK, DesEngine_run(engine));
    TEST_ASSERT_EQUAL_INT(1, engine->num_completed_entities);
    TEST_ASSERT_EQUAL_INT(5, engine->current_time);
    bool second_started_from_ready = false;
    for (int i = 0; i < engine->stats.num_transition_records; i++) {
        const DesTransitionRecord *record = &engine->stats.transition_records[i];
        if (record->stage_id == second && record->action_type == DES_ACTION_ACQUIRE_AND_PROCESS) {
            second_started_from_ready = record->from_state == ready && record->to_state == busy;
        }
    }
    TEST_ASSERT_TRUE(second_started_from_ready);
    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_delayed_arrival_sets_real_entry_time);
    RUN_TEST(test_max_time_does_not_process_future_event);
    RUN_TEST(test_max_events_is_processed_event_limit);
    RUN_TEST(test_second_run_does_not_seed_duplicate_entities);
    RUN_TEST(test_unconstrained_stage_supports_concurrent_entities);
    RUN_TEST(test_manual_initial_state_and_transition_trace);
    RUN_TEST(test_resource_contention_records_rejected_attempt);
    RUN_TEST(test_shared_resource_stages_keep_distinct_entity_state_machines);
    return UNITY_END();
}
