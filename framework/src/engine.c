#include "des/des_engine.h"
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

static void dispatchAction(DesEngine *engine, const DesEvent *event, DesActionType action_type);

DesEngine *DesEngine_create(const DesSimConfig *config) {
    DesEngine *engine = (DesEngine *)calloc(1, sizeof(DesEngine));
    engine->config = config;
    engine->current_time = 0;
    engine->next_event_id = 0;
    engine->next_entity_id = 0;
    engine->num_active_entities = 0;
    engine->num_completed_entities = 0;
    engine->running = false;
    engine->error = false;
    engine->last_error = DES_OK;

    DesEventQueue_init(&engine->queue, config->max_events > 0 ? config->max_events : DES_MAX_EVENTS);

    engine->num_stages = config->num_stages;
    engine->stages = (DesStage *)calloc((size_t)config->num_stages, sizeof(DesStage));

    for (int i = 0; i < config->num_stages; i++) {
        const DesStageDef *def = &config->stages[i];
        DesStage *s = &engine->stages[i];
        s->id = i;
        strncpy(s->name, def->name, DES_MAX_NAME - 1);
        s->resource_type_id = def->resource_type_id;
        s->num_states = def->num_states;
        s->num_event_types = def->num_event_types;
        s->processing_time = def->processing_time;
        s->num_outcomes = def->num_outcomes;
        memcpy(s->outcomes, def->outcomes, sizeof(DesStageOutcome) * (size_t)def->num_outcomes);
        s->current_state = 0;

        int fsm_size = s->num_states * s->num_event_types;
        if (fsm_size > 0) {
            s->fsm = (DesFsmEntry *)calloc((size_t)fsm_size, sizeof(DesFsmEntry));
            for (int j = 0; j < fsm_size; j++) {
                s->fsm[j].next_state = 0;
                s->fsm[j].action_type = DES_ACTION_NONE;
                s->fsm[j].custom_action_id = -1;
            }
        }

        for (int t = 0; t < def->num_transitions; t++) {
            const DesFsmTransition *tr = &def->transitions[t];
            int idx = tr->state_index * s->num_event_types + tr->event_index;
            if (idx < fsm_size) {
                s->fsm[idx].next_state = tr->next_state_index;
                s->fsm[idx].action_type = tr->action_type;
                s->fsm[idx].custom_action_id = tr->custom_action_id;
            }
        }
    }

    engine->num_resource_types = config->num_resources;
    engine->resource_types = (DesResourceType *)calloc((size_t)config->num_resources, sizeof(DesResourceType));
    engine->total_resource_instances = 0;
    for (int i = 0; i < config->num_resources; i++) {
        engine->resource_types[i].id = i;
        strncpy(engine->resource_types[i].name, config->resources[i].name, DES_MAX_NAME - 1);
        engine->resource_types[i].count = config->resources[i].count;
        engine->resource_types[i].available = config->resources[i].count;
        engine->resource_types[i].first_instance_idx = engine->total_resource_instances;
        engine->resource_types[i].instance_count = config->resources[i].count;
        engine->total_resource_instances += config->resources[i].count;
    }

    engine->resource_instances = (DesResourceInstance *)calloc(
        (size_t)(engine->total_resource_instances > 0 ? engine->total_resource_instances : 1),
        sizeof(DesResourceInstance));

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
        entity_cap = 100;
        for (int i = 0; i < config->num_arrivals; i++) {
            entity_cap += config->arrivals[i].entity_count;
        }
    }
    engine->entity_capacity = entity_cap;
    engine->entities = (DesEntity *)calloc((size_t)entity_cap, sizeof(DesEntity));

    DesStats_init(&engine->stats);

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
    free(engine->stages);
    free(engine->resource_types);
    free(engine->resource_instances);
    free(engine->entities);
    DesStats_destroy(&engine->stats);
    free(engine->custom_actions);
    free(engine);
}

int DesEngine_registerAction(DesEngine *engine, const char *name,
                             DesActionFn fn, void *user_data) {
    int id = engine->num_custom_actions++;
    engine->custom_actions = (DesCustomAction *)realloc(
        engine->custom_actions, (size_t)engine->num_custom_actions * sizeof(DesCustomAction));
    strncpy(engine->custom_actions[id].name, name, DES_MAX_NAME - 1);
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

static void dispatchAction(DesEngine *engine, const DesEvent *event, DesActionType action_type) {
    switch (action_type) {
        case DES_ACTION_ACQUIRE_AND_PROCESS: {
            DesStage *stage = &engine->stages[event->target_stage_id];
            int entity_id = event->entity_id;

            if (stage->resource_type_id != DES_INVALID_ID) {
                int instance = DesResource_acquire(engine, stage->resource_type_id);
                if (instance < 0) {
                    DesResourceType *rt = &engine->resource_types[stage->resource_type_id];
                    int retry_delay = 50;
                    int rstart = rt->first_instance_idx;
                    int rend = rstart + rt->instance_count;
                    for (int i = rstart; i < rend; i++) {
                        DesResourceInstance *ri = &engine->resource_instances[i];
                        if (ri->assigned_entity == DES_INVALID_ID &&
                            ri->available_at_time > engine->current_time) {
                            int wait = ri->available_at_time - (int)engine->current_time;
                            if (wait < retry_delay) retry_delay = wait;
                        }
                    }
                    DesEvent retry = *event;
                    retry.id = engine->next_event_id++;
                    retry.time = engine->current_time + retry_delay;
                    retry.priority = 1;
                    DesEventQueue_enqueue(&engine->queue, &retry);
                    return;
                }

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
                complete.event_type = 1;
                complete.entity_id = entity_id;
                complete.time = engine->current_time + delay;
                complete.priority = 0;
                complete.data[0] = instance;
                DesEventQueue_enqueue(&engine->queue, &complete);
            } else {
                DesEntity_enterStage(engine, entity_id, event->target_stage_id);

                int delay = DesRng_sample(engine, &stage->processing_time);
                if (delay < 1) delay = 1;

                DesEvent complete = *event;
                complete.id = engine->next_event_id++;
                complete.event_type = 1;
                complete.time = engine->current_time + delay;
                complete.priority = 0;
                DesEventQueue_enqueue(&engine->queue, &complete);
            }
            break;
        }

        case DES_ACTION_RELEASE_AND_DISPATCH: {
            DesStage *stage = &engine->stages[event->target_stage_id];
            int entity_id = event->entity_id;
            int instance_id = event->data[0];

            if (stage->resource_type_id != DES_INVALID_ID) {
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
                    next.time = engine->current_time;
                    next.priority = 0;
                    DesEventQueue_enqueue(&engine->queue, &next);
                }
            } else {
                DesStats_recordEntityTransition(engine, entity_id,
                                                event->target_stage_id,
                                                engine->entities[entity_id].stage_entry_time,
                                                engine->current_time,
                                                -1);
                DesEntity_exitSystem(engine, entity_id, -1);
            }
            break;
        }

        case DES_ACTION_RELEASE_AND_RETRY: {
            DesStage *stage = &engine->stages[event->target_stage_id];
            int instance_id = event->data[0];

            if (stage->resource_type_id != DES_INVALID_ID) {
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
            retry.event_type = 0;
            retry.entity_id = event->entity_id;
            retry.time = engine->current_time + 50;
            retry.priority = 1;
            DesEventQueue_enqueue(&engine->queue, &retry);
            break;
        }

        case DES_ACTION_WAIT_RETRY: {
            DesEvent retry = *event;
            retry.id = engine->next_event_id++;
            retry.time = engine->current_time + 50;
            retry.priority = 1;
            DesEventQueue_enqueue(&engine->queue, &retry);
            break;
        }

        case DES_ACTION_ENTITY_ENTER:
        case DES_ACTION_ENTITY_EXIT:
        case DES_ACTION_NONE:
        case DES_ACTION_CUSTOM:
            break;
    }
}

static void seedEntities(DesEngine *engine) {
    for (int a = 0; a < engine->config->num_arrivals; a++) {
        const DesEntityArrival *arr = &engine->config->arrivals[a];
        int cumulative_time = arr->start_time;

        for (int i = 0; i < arr->entity_count; i++) {
            if (i > 0) {
                cumulative_time += DesRng_sample(engine, &arr->inter_arrival);
            }
            if (cumulative_time < 0) cumulative_time = 0;

            int entity_id = DesEntity_create(engine, arr->entry_stage_id);
            if (entity_id == DES_INVALID_ID) continue;

            DesEvent ev = {0};
            ev.id = engine->next_event_id++;
            ev.target_stage_id = arr->entry_stage_id;
            ev.event_type = 0;
            ev.entity_id = entity_id;
            ev.data[0] = DES_INVALID_ID;
            ev.time = cumulative_time;
            ev.priority = arr->priority;
            DesEventQueue_enqueue(&engine->queue, &ev);
        }
    }
}

DesErrorCode DesEngine_run(DesEngine *engine) {
    seedEntities(engine);
    engine->running = true;

    while (engine->running && !engine->error) {
        if (engine->current_time >= engine->config->max_time) break;
        DesErrorCode ec = DesEngine_step(engine);
        if (ec == DES_ERR_QUEUE_EMPTY) break;
        if (ec != DES_OK) return ec;
    }
    return engine->last_error;
}

DesErrorCode DesEngine_step(DesEngine *engine) {
    if (DesEventQueue_isEmpty(&engine->queue)) {
        return DES_ERR_QUEUE_EMPTY;
    }

    DesEvent event;
    DesErrorCode ec = DesEventQueue_dequeue(&engine->queue, &event);
    if (ec != DES_OK) return ec;

    engine->current_time = event.time;

    if (event.target_stage_id < 0 || event.target_stage_id >= engine->num_stages) {
        return DES_ERR_NO_STAGE;
    }

    DesStage *stage = &engine->stages[event.target_stage_id];
    int *state_ptr = &stage->current_state;

    if (stage->resource_type_id != DES_INVALID_ID) {
        DesResourceType *rt = &engine->resource_types[stage->resource_type_id];
        if (event.data[0] >= 0) {
            int start = rt->first_instance_idx;
            int end = start + rt->instance_count;
            for (int i = start; i < end; i++) {
                DesResourceInstance *ri = &engine->resource_instances[i];
                if (ri->instance_id == event.data[0]) {
                    state_ptr = &ri->current_state;
                    break;
                }
            }
        } else {
            int start = rt->first_instance_idx;
            int end = start + rt->instance_count;
            for (int i = start; i < end; i++) {
                DesResourceInstance *ri = &engine->resource_instances[i];
                if (ri->assigned_entity == DES_INVALID_ID &&
                    ri->available_at_time <= engine->current_time) {
                    state_ptr = &ri->current_state;
                    break;
                }
            }
        }
    }

    int fsm_idx = (*state_ptr) * stage->num_event_types + event.event_type;
    int fsm_size = stage->num_states * stage->num_event_types;

    if (fsm_idx >= 0 && fsm_idx < fsm_size) {
        DesFsmEntry *entry = &stage->fsm[fsm_idx];
        if (entry->action_type == DES_ACTION_NONE && entry->next_state == 0 &&
            *state_ptr != 0) {
            DesEvent retry = event;
            retry.id = engine->next_event_id++;
            retry.time = engine->current_time + 50;
            retry.priority = 1;
            DesEventQueue_enqueue(&engine->queue, &retry);
        } else {
            dispatchAction(engine, &event, entry->action_type);
            *state_ptr = entry->next_state;
        }
    }

    if (engine->config->stats.record_events) {
        DesStats_recordEvent(engine, &event);
    }

    return DES_OK;
}
