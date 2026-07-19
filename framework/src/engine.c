#include "des/des_engine.h"
#include "des/des_config.h"
#include "des/des_event_queue.h"
#include "des/des_resource.h"
#include "des/des_entity.h"
#include "des/des_rng.h"
#include "des/des_stats.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

typedef struct {
    int resource_type_id;
    int resource_instance_id;
    int outcome_id;
} DesActionResult;

static bool dispatchAction(DesEngine *engine, const DesEvent *event,
                           DesActionType action_type, int custom_action_id,
                           DesActionResult *result);
static DesErrorCode seedEntities(DesEngine *engine);

static DesErrorCode scheduleEvent(DesEngine *engine, const DesEvent *event) {
    DesErrorCode ec = DesEventQueue_enqueue(&engine->queue, event);
    if (ec != DES_OK) {
        engine->error = true;
        engine->last_error = ec;
    }
    return ec;
}

DesEngine *DesEngine_create(const DesSimConfig *config) {
    DesValidationResult validation;
    if (!DesConfig_validate(config, &validation)) return NULL;

    DesEngine *engine = (DesEngine *)calloc(1, sizeof(DesEngine));
    if (!engine) return NULL;
    engine->config = config;
    engine->current_time = 0;
    engine->next_event_id = 0;
    engine->events_processed = 0;
    engine->next_entity_id = 0;
    engine->num_active_entities = 0;
    engine->num_completed_entities = 0;
    engine->running = false;
    engine->seeded = false;
    engine->error = false;
    engine->last_error = DES_OK;

    DesEventQueue_init(&engine->queue, 256);
    if (!engine->queue.buffer) {
        free(engine);
        return NULL;
    }

    engine->num_stages = config->num_stages;
    engine->stages = (DesStage *)calloc((size_t)config->num_stages, sizeof(DesStage));
    if (!engine->stages) {
        DesEngine_destroy(engine);
        return NULL;
    }

    for (int i = 0; i < config->num_stages; i++) {
        const DesStageDef *def = &config->stages[i];
        DesStage *s = &engine->stages[i];
        s->id = i;
        strncpy(s->name, def->name, DES_MAX_NAME - 1);
        s->name[DES_MAX_NAME - 1] = '\0';
        s->resource_type_id = def->resource_type_id;
        s->initial_state = def->initial_state_index;
        s->num_states = def->num_states;
        s->num_event_types = def->num_event_types;
        s->processing_time = def->processing_time;
        s->num_outcomes = def->num_outcomes;
        memcpy(s->outcomes, def->outcomes, sizeof(DesStageOutcome) * (size_t)def->num_outcomes);
        s->current_state = 0;
        s->entry_event_type = 0;
        s->completion_event_type = DES_INVALID_ID;

        int fsm_size = s->num_states * s->num_event_types;
        if (fsm_size > 0) {
            s->fsm = (DesFsmEntry *)calloc((size_t)fsm_size, sizeof(DesFsmEntry));
            if (!s->fsm) {
                DesEngine_destroy(engine);
                return NULL;
            }
            for (int j = 0; j < fsm_size; j++) {
                s->fsm[j].next_state = 0;
                s->fsm[j].action_type = DES_ACTION_NONE;
                s->fsm[j].custom_action_id = -1;
                s->fsm[j].defined = false;
            }
        }

        for (int t = 0; t < def->num_transitions; t++) {
            const DesFsmTransition *tr = &def->transitions[t];
            int idx = tr->state_index * s->num_event_types + tr->event_index;
            if (idx < fsm_size) {
                s->fsm[idx].next_state = tr->next_state_index;
                s->fsm[idx].action_type = tr->action_type;
                s->fsm[idx].custom_action_id = tr->custom_action_id;
                s->fsm[idx].defined = true;
                if (tr->action_type == DES_ACTION_ACQUIRE_AND_PROCESS ||
                    tr->action_type == DES_ACTION_ENTITY_ENTER)
                    s->entry_event_type = tr->event_index;
                if (tr->action_type == DES_ACTION_RELEASE_AND_DISPATCH)
                    s->completion_event_type = tr->event_index;
            }
        }
        if (s->completion_event_type == DES_INVALID_ID && s->num_event_types > 1)
            s->completion_event_type = 1;
    }

    engine->num_resource_types = config->num_resources;
    engine->resource_types = (DesResourceType *)calloc((size_t)config->num_resources, sizeof(DesResourceType));
    if (config->num_resources > 0 && !engine->resource_types) {
        DesEngine_destroy(engine);
        return NULL;
    }
    engine->total_resource_instances = 0;
    for (int i = 0; i < config->num_resources; i++) {
        engine->resource_types[i].id = i;
        strncpy(engine->resource_types[i].name, config->resources[i].name, DES_MAX_NAME - 1);
        engine->resource_types[i].name[DES_MAX_NAME - 1] = '\0';
        engine->resource_types[i].count = config->resources[i].count;
        engine->resource_types[i].available = config->resources[i].count;
        engine->resource_types[i].first_instance_idx = engine->total_resource_instances;
        engine->resource_types[i].instance_count = config->resources[i].count;
        engine->total_resource_instances += config->resources[i].count;
    }

    engine->resource_instances = (DesResourceInstance *)calloc(
        (size_t)(engine->total_resource_instances > 0 ? engine->total_resource_instances : 1),
        sizeof(DesResourceInstance));
    if (!engine->resource_instances) {
        DesEngine_destroy(engine);
        return NULL;
    }

    int idx = 0;
    for (int i = 0; i < config->num_resources; i++) {
        for (int j = 0; j < config->resources[i].count; j++) {
            DesResourceInstance *ri = &engine->resource_instances[idx];
            ri->type_id = i;
            ri->instance_id = j;
            ri->current_state = 0;
            ri->assigned_entity = DES_INVALID_ID;
            ri->assigned_stage = DES_INVALID_ID;
            ri->available_at_time = config->resources[i].available_at;
            idx++;
        }
    }

    int entity_cap = config->entity_capacity;
    if (entity_cap <= 0) {
        entity_cap = 0;
        for (int i = 0; i < config->num_arrivals; i++) {
            entity_cap += config->arrivals[i].entity_count;
        }
    }
    engine->entity_capacity = entity_cap;
    engine->entities = (DesEntity *)calloc((size_t)entity_cap, sizeof(DesEntity));
    if (entity_cap > 0 && !engine->entities) {
        DesEngine_destroy(engine);
        return NULL;
    }

    DesStats_init(&engine->stats);
    if (!engine->stats.entity_records || !engine->stats.resource_records ||
        !engine->stats.transition_records) {
        DesEngine_destroy(engine);
        return NULL;
    }

    engine->custom_actions = NULL;
    engine->num_custom_actions = 0;

    unsigned int seed = config->seed;
    if (seed == 0) seed = (unsigned int)time(NULL);
    DesRng_setSeed(engine, seed);

    return engine;
}

void DesEngine_destroy(DesEngine *engine) {
    if (!engine) return;
    DesEventQueue_destroy(&engine->queue);
    if (engine->stages) {
        for (int i = 0; i < engine->num_stages; i++) free(engine->stages[i].fsm);
        free(engine->stages);
    }
    free(engine->resource_types);
    free(engine->resource_instances);
    free(engine->entities);
    DesStats_destroy(&engine->stats);
    free(engine->custom_actions);
    free(engine);
}

int DesEngine_registerAction(DesEngine *engine, const char *name,
                             DesActionFn fn, void *user_data) {
    if (!engine || !name || !fn) return DES_INVALID_ID;
    int id = engine->num_custom_actions;
    DesCustomAction *actions = (DesCustomAction *)realloc(
        engine->custom_actions, (size_t)(id + 1) * sizeof(DesCustomAction));
    if (!actions) return DES_INVALID_ID;
    engine->custom_actions = actions;
    engine->num_custom_actions++;
    strncpy(engine->custom_actions[id].name, name, DES_MAX_NAME - 1);
    engine->custom_actions[id].name[DES_MAX_NAME - 1] = '\0';
    engine->custom_actions[id].fn = fn;
    engine->custom_actions[id].user_data = user_data;
    return id;
}

int DesEngine_getTime(const DesEngine *engine) {
    return engine->current_time;
}

int DesEngine_getEntityCount(const DesEngine *engine) {
    return engine->next_entity_id;
}

int DesEngine_getResourceAvailable(const DesEngine *engine, int resource_type_id) {
    return DesResource_getAvailable(engine, resource_type_id);
}

const char *DesEngine_getStageName(const DesEngine *engine, int stage_id) {
    if (stage_id < 0 || stage_id >= engine->num_stages) return "UNKNOWN";
    return engine->stages[stage_id].name;
}

static bool dispatchAction(DesEngine *engine, const DesEvent *event,
                           DesActionType action_type, int custom_action_id,
                           DesActionResult *result) {
    switch (action_type) {
        case DES_ACTION_ACQUIRE_AND_PROCESS: {
            DesStage *stage = &engine->stages[event->target_stage_id];
            int entity_id = event->entity_id;

            if (stage->resource_type_id != DES_INVALID_ID) {
                int instance = DesResource_acquire(engine, stage->resource_type_id);
                if (instance < 0) {
                    DesResourceType *rt = &engine->resource_types[stage->resource_type_id];
                    int retry_delay = 50;
                    int next_scheduled_availability = INT32_MAX;
                    bool has_busy_instance = false;
                    int rstart = rt->first_instance_idx;
                    int rend = rstart + rt->instance_count;
                    for (int i = rstart; i < rend; i++) {
                        DesResourceInstance *ri = &engine->resource_instances[i];
                        if (ri->assigned_entity != DES_INVALID_ID) has_busy_instance = true;
                        else if (ri->available_at_time > engine->current_time &&
                                 ri->available_at_time < next_scheduled_availability)
                            next_scheduled_availability = ri->available_at_time;
                    }
                    if (!has_busy_instance && next_scheduled_availability != INT32_MAX)
                        retry_delay = next_scheduled_availability - engine->current_time;
                    else if (next_scheduled_availability != INT32_MAX &&
                            next_scheduled_availability - engine->current_time < retry_delay)
                        retry_delay = next_scheduled_availability - engine->current_time;
                    if (retry_delay < 1) retry_delay = 1;
                    DesEvent retry = *event;
                    retry.id = engine->next_event_id++;
                    retry.time = engine->current_time + retry_delay;
                    retry.priority = 1;
                    scheduleEvent(engine, &retry);
                    return false;
                }

                result->resource_type_id = stage->resource_type_id;
                result->resource_instance_id = instance;

                {
                    DesResourceType *rt = &engine->resource_types[stage->resource_type_id];
                    int rstart = rt->first_instance_idx;
                    int rend = rstart + rt->instance_count;
                    for (int i = rstart; i < rend; i++) {
                        DesResourceInstance *ri = &engine->resource_instances[i];
                        if (ri->instance_id == instance) {
                            ri->assigned_entity = entity_id;
                            ri->assigned_stage = event->target_stage_id;
                            break;
                        }
                    }
                }

                DesEntity_enterStage(engine, entity_id, event->target_stage_id);

                if (engine->config->stats.record_resource_util) {
                    DesStats_recordResourceState(engine, engine->current_time,
                                                 stage->resource_type_id, instance,
                                                 1, entity_id);
                }

                int delay = DesRng_sample(engine, &stage->processing_time);
                if (delay < 1) delay = 1;

                DesEvent complete = {0};
                complete.id = engine->next_event_id++;
                complete.target_stage_id = event->target_stage_id;
                complete.event_type = stage->completion_event_type;
                complete.entity_id = entity_id;
                complete.time = engine->current_time + delay;
                complete.priority = 0;
                complete.data[0] = instance;
                if (scheduleEvent(engine, &complete) != DES_OK) return false;
            } else {
                DesEntity_enterStage(engine, entity_id, event->target_stage_id);

                int delay = DesRng_sample(engine, &stage->processing_time);
                if (delay < 1) delay = 1;

                DesEvent complete = *event;
                complete.id = engine->next_event_id++;
                complete.event_type = stage->completion_event_type;
                complete.time = engine->current_time + delay;
                complete.priority = 0;
                if (scheduleEvent(engine, &complete) != DES_OK) return false;
            }
            return true;
        }

        case DES_ACTION_RELEASE_AND_DISPATCH: {
            DesStage *stage = &engine->stages[event->target_stage_id];
            int entity_id = event->entity_id;
            int instance_id = event->data[0];

            if (stage->resource_type_id != DES_INVALID_ID) {
                result->resource_type_id = stage->resource_type_id;
                result->resource_instance_id = instance_id;
                DesResource_release(engine, stage->resource_type_id, instance_id);
                if (engine->config->stats.record_resource_util) {
                    DesStats_recordResourceState(engine, engine->current_time,
                                                 stage->resource_type_id, instance_id,
                                                 0, DES_INVALID_ID);
                }
            }

            if (stage->num_outcomes > 0) {
                int outcome_idx = DesRng_selectOutcome(engine, stage->outcomes,
                                                       stage->num_outcomes);
                DesStageOutcome *outcome = &stage->outcomes[outcome_idx];
                result->outcome_id = outcome_idx;

                if (engine->config->stats.record_entity_flow)
                    DesStats_recordEntityTransition(engine, entity_id,
                                                    event->target_stage_id,
                                                    engine->entities[entity_id].stage_entry_time,
                                                    engine->current_time,
                                                    outcome_idx);

                if (outcome->next_stage_id == DES_INVALID_ID) {
                    DesEntity_exitSystem(engine, entity_id, outcome_idx);
                } else {
                    DesEvent next = {0};
                    next.id = engine->next_event_id++;
                    next.target_stage_id = outcome->next_stage_id;
                    next.event_type = outcome->next_event_index;
                    next.entity_id = entity_id;
                    next.data[0] = DES_INVALID_ID;
                    next.data[1] = 1;
                    next.time = engine->current_time;
                    next.priority = 0;
                    if (scheduleEvent(engine, &next) != DES_OK) return false;
                }
            } else {
                if (engine->config->stats.record_entity_flow)
                    DesStats_recordEntityTransition(engine, entity_id,
                                                    event->target_stage_id,
                                                    engine->entities[entity_id].stage_entry_time,
                                                    engine->current_time,
                                                    -1);
                DesEntity_exitSystem(engine, entity_id, -1);
            }
            return true;
        }

        case DES_ACTION_RELEASE_AND_RETRY: {
            DesStage *stage = &engine->stages[event->target_stage_id];
            int instance_id = event->data[0];

            if (stage->resource_type_id != DES_INVALID_ID) {
                result->resource_type_id = stage->resource_type_id;
                result->resource_instance_id = instance_id;
                DesResource_release(engine, stage->resource_type_id, instance_id);
                if (engine->config->stats.record_resource_util) {
                    DesStats_recordResourceState(engine, engine->current_time,
                                                 stage->resource_type_id, instance_id,
                                                 0, DES_INVALID_ID);
                }
            }

            DesEvent retry = {0};
            retry.id = engine->next_event_id++;
            retry.target_stage_id = event->target_stage_id;
            retry.event_type = stage->entry_event_type;
            retry.entity_id = event->entity_id;
            retry.time = engine->current_time + 50;
            retry.priority = 1;
            if (scheduleEvent(engine, &retry) != DES_OK) return false;
            return true;
        }

        case DES_ACTION_WAIT_RETRY: {
            DesEvent retry = *event;
            retry.id = engine->next_event_id++;
            retry.time = engine->current_time + 50;
            retry.priority = 1;
            if (scheduleEvent(engine, &retry) != DES_OK) return false;
            return true;
        }

        case DES_ACTION_ENTITY_ENTER:
            DesEntity_enterStage(engine, event->entity_id, event->target_stage_id);
            return true;
        case DES_ACTION_ENTITY_EXIT:
            DesEntity_exitSystem(engine, event->entity_id, -1);
            return true;
        case DES_ACTION_NONE:
            return true;
        case DES_ACTION_CUSTOM:
            if (custom_action_id < 0 || custom_action_id >= engine->num_custom_actions) {
                engine->error = true;
                engine->last_error = DES_ERR_CONFIG;
                return false;
            }
            engine->custom_actions[custom_action_id].fn(
                engine, event, engine->custom_actions[custom_action_id].user_data);
            return true;
    }
    engine->error = true;
    engine->last_error = DES_ERR_CONFIG;
    return false;
}

static DesErrorCode seedEntities(DesEngine *engine) {
    if (engine->seeded) return DES_OK;
    for (int a = 0; a < engine->config->num_arrivals; a++) {
        const DesEntityArrival *arr = &engine->config->arrivals[a];
        int cumulative_time = arr->start_time;

        for (int i = 0; i < arr->entity_count; i++) {
            if (i > 0) {
                cumulative_time += DesRng_sample(engine, &arr->inter_arrival);
            }
            if (cumulative_time < 0) cumulative_time = 0;

            DesEvent ev = {0};
            ev.id = engine->next_event_id++;
            ev.target_stage_id = arr->entry_stage_id;
            ev.event_type = engine->stages[arr->entry_stage_id].entry_event_type;
            ev.entity_id = DES_INVALID_ID;
            ev.data[0] = DES_INVALID_ID;
            ev.time = cumulative_time;
            ev.priority = arr->priority;
            DesErrorCode ec = scheduleEvent(engine, &ev);
            if (ec != DES_OK) return ec;
        }
    }
    engine->seeded = true;
    return DES_OK;
}

DesErrorCode DesEngine_run(DesEngine *engine) {
    if (!engine) return DES_ERR_NULL_POINTER;
    DesErrorCode seed_error = seedEntities(engine);
    if (seed_error != DES_OK) return seed_error;
    engine->running = true;

    while (engine->running && !engine->error) {
        if (engine->events_processed >= engine->config->max_events) break;
        const DesEvent *next = DesEventQueue_peek(&engine->queue);
        if (!next) break;
        if (next->time > engine->config->max_time) break;
        DesErrorCode ec = DesEngine_step(engine);
        if (ec == DES_ERR_QUEUE_EMPTY) break;
        if (ec != DES_OK) return ec;
    }
    return engine->last_error;
}

DesErrorCode DesEngine_step(DesEngine *engine) {
    if (!engine) return DES_ERR_NULL_POINTER;
    DesErrorCode seed_error = seedEntities(engine);
    if (seed_error != DES_OK) return seed_error;
    if (engine->events_processed >= engine->config->max_events) return DES_OK;
    if (DesEventQueue_isEmpty(&engine->queue)) {
        return DES_ERR_QUEUE_EMPTY;
    }

    DesEvent event;
    DesErrorCode ec = DesEventQueue_dequeue(&engine->queue, &event);
    if (ec != DES_OK) return ec;

    engine->current_time = event.time;
    engine->events_processed++;

    if (event.target_stage_id < 0 || event.target_stage_id >= engine->num_stages) {
        engine->error = true;
        engine->last_error = DES_ERR_NO_STAGE;
        return DES_ERR_NO_STAGE;
    }

    if (event.entity_id == DES_INVALID_ID) {
        event.entity_id = DesEntity_create(engine, event.target_stage_id);
        if (event.entity_id == DES_INVALID_ID) {
            engine->error = true;
            engine->last_error = DES_ERR_ENTITY_FULL;
            return engine->last_error;
        }
    }

    DesStage *stage = &engine->stages[event.target_stage_id];
    DesEntity *entity = &engine->entities[event.entity_id];
    if (event.data[1] == 1 || entity->current_stage_id != event.target_stage_id)
        entity->current_state = stage->initial_state;
    int *state_ptr = &entity->current_state;

    int fsm_idx = (*state_ptr) * stage->num_event_types + event.event_type;
    int fsm_size = stage->num_states * stage->num_event_types;

    if (fsm_idx < 0 || fsm_idx >= fsm_size || !stage->fsm[fsm_idx].defined) {
        engine->error = true;
        engine->last_error = DES_ERR_CONFIG;
        return engine->last_error;
    } else {
        DesFsmEntry *entry = &stage->fsm[fsm_idx];
        int from_state = *state_ptr;
        DesActionResult result = { DES_INVALID_ID, DES_INVALID_ID, DES_INVALID_ID };
        bool accepted = dispatchAction(engine, &event, entry->action_type,
                                       entry->custom_action_id, &result);
        if (accepted)
            *state_ptr = entry->next_state;
        if (engine->config->stats.record_events) {
            DesTransitionRecord record = {
                engine->current_time, event.id, event.entity_id,
                event.target_stage_id, event.event_type, from_state,
                accepted ? entry->next_state : from_state,
                entry->action_type, result.resource_type_id,
                result.resource_instance_id, result.outcome_id, accepted
            };
            DesStats_recordTransition(engine, &record);
        }
    }

    if (engine->config->stats.record_events) {
        DesStats_recordEvent(engine, &event);
    }

    return engine->error ? engine->last_error : DES_OK;
}
