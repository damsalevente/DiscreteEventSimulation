#include "des/des_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *DesError_toString(DesErrorCode code) {
    switch (code) {
        case DES_OK:                return "OK";
        case DES_ERR_QUEUE_FULL:    return "event queue full";
        case DES_ERR_QUEUE_EMPTY:   return "event queue empty";
        case DES_ERR_NULL_POINTER:  return "null pointer";
        case DES_ERR_FILE_IO:       return "file I/O error";
        case DES_ERR_CONFIG:        return "invalid configuration";
        case DES_ERR_NO_RESOURCE:   return "resource not found";
        case DES_ERR_NO_STAGE:      return "stage not found";
        case DES_ERR_NO_OUTCOME:    return "no outcome defined";
        case DES_ERR_OUT_OF_MEMORY: return "out of memory";
        case DES_ERR_ENTITY_FULL:   return "entity capacity exceeded";
    }
    return "unknown error";
}

void DesConfig_initValue(DesSimConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->name, "Untitled scenario", sizeof(cfg->name) - 1);
    cfg->max_time = 100000;
    cfg->max_events = DES_MAX_EVENTS;
    cfg->entity_capacity = 0;
    cfg->seed = 0;
    cfg->stats.record_events = true;
    cfg->stats.record_entity_flow = true;
    cfg->stats.record_resource_util = false;
    strncpy(cfg->stats.output_dir, "./output", sizeof(cfg->stats.output_dir) - 1);
}

DesSimConfig *DesConfig_create(void) {
    DesSimConfig *cfg = (DesSimConfig *)malloc(sizeof(DesSimConfig));
    if (!cfg) return NULL;
    DesConfig_initValue(cfg);
    return cfg;
}

void DesConfig_destroy(DesSimConfig *cfg) {
    free(cfg);
}

const char *DesConfig_getLastError(const DesSimConfig *cfg) {
    if (!cfg) return "null config";
    return cfg->last_error;
}

static void set_error(DesSimConfig *cfg, const char *msg) {
    if (cfg) {
        strncpy(cfg->last_error, msg, sizeof(cfg->last_error) - 1);
        cfg->last_error[sizeof(cfg->last_error) - 1] = '\0';
    }
}

int DesConfig_addResource(DesSimConfig *cfg, const char *name, int count) {
    if (!cfg || !name) { if (cfg) set_error(cfg, "null argument"); return DES_INVALID_ID; }
    int id = cfg->num_resources;
    if (id >= DES_MAX_RESOURCES) { set_error(cfg, "too many resources"); return DES_INVALID_ID; }
    if (name[0] == '\0' || count < 1) {
        set_error(cfg, "resource name must be non-empty and count must be positive");
        return DES_INVALID_ID;
    }
    strncpy(cfg->resources[id].name, name, DES_MAX_NAME - 1);
    cfg->resources[id].name[DES_MAX_NAME - 1] = '\0';
    cfg->resources[id].count = count;
    cfg->resources[id].available_at = 0;
    cfg->num_resources++;
    return id;
}

void DesConfig_setResourceAvailableAt(DesSimConfig *cfg, int resource_id, int time) {
    if (!cfg || resource_id < 0 || resource_id >= cfg->num_resources) return;
    cfg->resources[resource_id].available_at = time;
}

int DesConfig_addStage(DesSimConfig *cfg, const char *name) {
    if (!cfg || !name) { if (cfg) set_error(cfg, "null argument"); return DES_INVALID_ID; }
    if (name[0] == '\0') { set_error(cfg, "stage name must be non-empty"); return DES_INVALID_ID; }
    int id = cfg->num_stages;
    if (id >= DES_MAX_STAGES) { set_error(cfg, "too many stages"); return DES_INVALID_ID; }
    memset(&cfg->stages[id], 0, sizeof(DesStageDef));
    strncpy(cfg->stages[id].name, name, DES_MAX_NAME - 1);
    cfg->stages[id].name[DES_MAX_NAME - 1] = '\0';
    cfg->stages[id].resource_type_id = DES_INVALID_ID;
    cfg->stages[id].initial_state_index = 0;
    cfg->num_stages++;
    return id;
}

void DesStage_setResource(DesSimConfig *cfg, int stage_id, int resource_type_id) {
    if (!cfg || stage_id < 0 || stage_id >= cfg->num_stages) return;
    if (resource_type_id != DES_INVALID_ID &&
        (resource_type_id < 0 || resource_type_id >= cfg->num_resources)) {
        set_error(cfg, "resource index is out of range");
        return;
    }
    cfg->stages[stage_id].resource_type_id = resource_type_id;
}

void DesStage_setProcessingTime(DesSimConfig *cfg, int stage_id,
                                DesDistType dist, double p1, double p2) {
    if (!cfg || stage_id < 0 || stage_id >= cfg->num_stages) return;
    cfg->stages[stage_id].processing_time.type = dist;
    cfg->stages[stage_id].processing_time.param1 = p1;
    cfg->stages[stage_id].processing_time.param2 = p2;
}

void DesStage_setInitialState(DesSimConfig *cfg, int stage_id, int state_id) {
    if (!cfg || stage_id < 0 || stage_id >= cfg->num_stages) return;
    DesStageDef *stage = &cfg->stages[stage_id];
    if (state_id < 0 || state_id >= stage->num_states) {
        set_error(cfg, "initial state index is out of range");
        return;
    }
    stage->initial_state_index = state_id;
}

int DesStage_addState(DesSimConfig *cfg, int stage_id, const char *state_name) {
    if (!cfg || !state_name || stage_id < 0 || stage_id >= cfg->num_stages) return DES_INVALID_ID;
    DesStageDef *s = &cfg->stages[stage_id];
    if (s->num_states >= DES_MAX_STATES) return DES_INVALID_ID;
    int id = s->num_states;
    strncpy(s->state_names[id], state_name, DES_MAX_NAME - 1);
    s->state_names[id][DES_MAX_NAME - 1] = '\0';
    s->num_states++;
    return id;
}

int DesStage_addEventType(DesSimConfig *cfg, int stage_id, const char *event_name) {
    if (!cfg || !event_name || stage_id < 0 || stage_id >= cfg->num_stages) return DES_INVALID_ID;
    DesStageDef *s = &cfg->stages[stage_id];
    if (s->num_event_types >= DES_MAX_EVENT_TYPES) return DES_INVALID_ID;
    int id = s->num_event_types;
    strncpy(s->event_type_names[id], event_name, DES_MAX_NAME - 1);
    s->event_type_names[id][DES_MAX_NAME - 1] = '\0';
    s->num_event_types++;
    return id;
}

void DesStage_addTransitionIdx(DesSimConfig *cfg, int stage_id,
                               int from_state, int event, int to_state,
                               DesActionType action) {
    if (!cfg || stage_id < 0 || stage_id >= cfg->num_stages) return;
    DesStageDef *s = &cfg->stages[stage_id];
    if (s->num_transitions >= DES_MAX_STATES * DES_MAX_EVENT_TYPES) return;

    DesFsmTransition *t = &s->transitions[s->num_transitions];
    t->state_index = from_state;
    t->event_index = event;
    t->next_state_index = to_state;
    t->action_type = action;
    t->custom_action_id = 0;
    s->num_transitions++;
}

void DesStage_addOutcomeIdx(DesSimConfig *cfg, int stage_id,
                            double probability, int next_stage_id,
                            int next_event_index, const char *outcome_name) {
    if (!cfg || !outcome_name || stage_id < 0 || stage_id >= cfg->num_stages) return;
    DesStageDef *s = &cfg->stages[stage_id];
    if (s->num_outcomes >= DES_MAX_OUTCOMES) return;

    DesStageOutcome *o = &s->outcomes[s->num_outcomes];
    strncpy(o->name, outcome_name, DES_MAX_NAME - 1);
    o->name[DES_MAX_NAME - 1] = '\0';
    o->probability = probability;
    o->next_stage_id = next_stage_id;
    o->next_event_index = next_event_index;
    s->num_outcomes++;
}

int DesConfig_addArrivalIdx(DesSimConfig *cfg, const char *entity_name,
                            int count, int entry_stage_id,
                            DesDistType dist, double p1, double p2) {
    if (!cfg || !entity_name || entity_name[0] == '\0' || count < 1 ||
        entry_stage_id < 0 || entry_stage_id >= cfg->num_stages) {
        if (cfg) set_error(cfg, "arrival requires a name, positive count, and valid entry stage");
        return DES_INVALID_ID;
    }
    int id = cfg->num_arrivals;
    if (id >= DES_MAX_ARRIVALS) { set_error(cfg, "too many arrivals"); return DES_INVALID_ID; }
    DesEntityArrival *a = &cfg->arrivals[id];
    strncpy(a->name, entity_name, DES_MAX_NAME - 1);
    a->name[DES_MAX_NAME - 1] = '\0';
    a->entity_count = count;
    a->entry_stage_id = entry_stage_id;
    a->start_time = 0;
    a->priority = 0;
    a->inter_arrival.type = dist;
    a->inter_arrival.param1 = p1;
    a->inter_arrival.param2 = p2;
    cfg->num_arrivals++;
    return id;
}

/* --- String-based (resolves names to indices) --- */

static int findStateIndex(const DesStageDef *s, const char *name) {
    for (int i = 0; i < s->num_states; i++) {
        if (strcmp(s->state_names[i], name) == 0) return i;
    }
    return DES_INVALID_ID;
}

static int findEventIndex(const DesStageDef *s, const char *name) {
    for (int i = 0; i < s->num_event_types; i++) {
        if (strcmp(s->event_type_names[i], name) == 0) return i;
    }
    return DES_INVALID_ID;
}

void DesStage_addTransition(DesSimConfig *cfg, int stage_id,
                            const char *from_state, const char *event_name,
                            const char *to_state, DesActionType action) {
    if (stage_id < 0 || stage_id >= cfg->num_stages) return;
    DesStageDef *s = &cfg->stages[stage_id];

    int si = findStateIndex(s, from_state);
    int ei = findEventIndex(s, event_name);
    int ti = findStateIndex(s, to_state);
    if (si == DES_INVALID_ID || ei == DES_INVALID_ID || ti == DES_INVALID_ID) {
        char buf[256];
        snprintf(buf, sizeof(buf), "stage '%s': unknown state or event (from='%s', event='%s', to='%s')",
                 s->name, from_state, event_name, to_state);
        set_error(cfg, buf);
        return;
    }

    DesStage_addTransitionIdx(cfg, stage_id, si, ei, ti, action);
}

void DesStage_addOutcome(DesSimConfig *cfg, int stage_id,
                         double probability, const char *next_stage_name,
                         const char *next_event_name, const char *outcome_name) {
    if (stage_id < 0 || stage_id >= cfg->num_stages) return;

    int next_stage_id = DES_INVALID_ID;
    int next_event_index = 0;
    if (next_stage_name) {
        for (int i = 0; i < cfg->num_stages; i++) {
            if (strcmp(cfg->stages[i].name, next_stage_name) == 0) {
                next_stage_id = i;
                break;
            }
        }
        if (next_stage_id == DES_INVALID_ID) {
            char buf[256];
            snprintf(buf, sizeof(buf), "stage '%s': unknown next_stage '%s'",
                     cfg->stages[stage_id].name, next_stage_name);
            set_error(cfg, buf);
            return;
        }
    }
    if (next_event_name && next_stage_id != DES_INVALID_ID) {
        DesStageDef *target = &cfg->stages[next_stage_id];
        bool found = false;
        for (int i = 0; i < target->num_event_types; i++) {
            if (strcmp(target->event_type_names[i], next_event_name) == 0) {
                next_event_index = i;
                found = true;
                break;
            }
        }
        if (!found) {
            char buf[256];
            snprintf(buf, sizeof(buf), "stage '%s': unknown next_event '%s' on stage '%s'",
                     cfg->stages[stage_id].name, next_event_name, target->name);
            set_error(cfg, buf);
            return;
        }
    }

    DesStage_addOutcomeIdx(cfg, stage_id, probability, next_stage_id,
                           next_event_index, outcome_name);
}

int DesConfig_addArrival(DesSimConfig *cfg, const char *entity_name,
                          int count, const char *entry_stage,
                          DesDistType dist, double p1, double p2) {
    if (!cfg || !entry_stage) {
        if (cfg) set_error(cfg, "arrival requires an entry stage");
        return DES_INVALID_ID;
    }
    int entry_stage_id = DES_INVALID_ID;
    for (int i = 0; i < cfg->num_stages; i++) {
        if (strcmp(cfg->stages[i].name, entry_stage) == 0) {
            entry_stage_id = i;
            break;
        }
    }
    if (entry_stage_id == DES_INVALID_ID) {
        char buf[256];
        snprintf(buf, sizeof(buf), "unknown entry_stage '%s'", entry_stage);
        set_error(cfg, buf);
        return DES_INVALID_ID;
    }
    return DesConfig_addArrivalIdx(cfg, entity_name, count, entry_stage_id,
                                   dist, p1, p2);
}

/* --- Mode shorthand (must come after all primitives it calls) --- */

void DesStage_setResourceMode(DesSimConfig *cfg, int stage_id, int resource_type_id,
                              DesDistType dist, double p1, double p2) {
    if (stage_id < 0 || stage_id >= cfg->num_stages) return;
    DesStageDef *s = &cfg->stages[stage_id];

    s->resource_type_id = resource_type_id;
    s->initial_state_index = 0;

    DesStage_addState(cfg, stage_id, "IDLE");
    DesStage_addState(cfg, stage_id, "BUSY");
    DesStage_addEventType(cfg, stage_id, "ENTER");
    DesStage_addEventType(cfg, stage_id, "COMPLETE");

    s->processing_time.type = dist;
    s->processing_time.param1 = p1;
    s->processing_time.param2 = p2;

    DesStage_addTransition(cfg, stage_id, "IDLE", "ENTER", "BUSY",
                           DES_ACTION_ACQUIRE_AND_PROCESS);
    DesStage_addTransition(cfg, stage_id, "BUSY", "COMPLETE", "IDLE",
                           DES_ACTION_RELEASE_AND_DISPATCH);
}

/* --- Setters --- */

void DesConfig_setArrivalStart(DesSimConfig *cfg, int arrival_id, int start_time) {
    if (!cfg || arrival_id < 0 || arrival_id >= cfg->num_arrivals) return;
    cfg->arrivals[arrival_id].start_time = start_time;
}

void DesConfig_setArrivalPriority(DesSimConfig *cfg, int arrival_id, int priority) {
    if (!cfg || arrival_id < 0 || arrival_id >= cfg->num_arrivals) return;
    cfg->arrivals[arrival_id].priority = priority;
}

void DesConfig_setMaxTime(DesSimConfig *cfg, int max_time) {
    if (!cfg) return;
    cfg->max_time = max_time;
}

void DesConfig_setMaxEvents(DesSimConfig *cfg, int max_events) {
    if (!cfg) return;
    cfg->max_events = max_events;
}

void DesConfig_setEntityCapacity(DesSimConfig *cfg, int capacity) {
    if (!cfg) return;
    cfg->entity_capacity = capacity;
}

void DesConfig_setSeed(DesSimConfig *cfg, unsigned int seed) {
    if (!cfg) return;
    cfg->seed = seed;
}

void DesConfig_setName(DesSimConfig *cfg, const char *name) {
    if (!cfg || !name || name[0] == '\0') return;
    strncpy(cfg->name, name, DES_MAX_NAME - 1);
    cfg->name[DES_MAX_NAME - 1] = '\0';
}

void DesConfig_setStats(DesSimConfig *cfg, bool record_events,
                        bool record_entity_flow, bool record_resource_util,
                        const char *output_dir) {
    if (!cfg) return;
    cfg->stats.record_events = record_events;
    cfg->stats.record_entity_flow = record_entity_flow;
    cfg->stats.record_resource_util = record_resource_util;
    if (output_dir) {
        strncpy(cfg->stats.output_dir, output_dir, sizeof(cfg->stats.output_dir) - 1);
    }
}

/* --- Setters for editing --- */

void DesConfig_setResourceName(DesSimConfig *cfg, int resource_id, const char *name) {
    if (resource_id < 0 || resource_id >= cfg->num_resources || !name) return;
    strncpy(cfg->resources[resource_id].name, name, DES_MAX_NAME - 1);
    cfg->resources[resource_id].name[DES_MAX_NAME - 1] = '\0';
}

void DesConfig_setResourceCount(DesSimConfig *cfg, int resource_id, int count) {
    if (resource_id < 0 || resource_id >= cfg->num_resources) return;
    if (count < 1) count = 1;
    cfg->resources[resource_id].count = count;
}

void DesStage_setName(DesSimConfig *cfg, int stage_id, const char *name) {
    if (stage_id < 0 || stage_id >= cfg->num_stages || !name) return;
    strncpy(cfg->stages[stage_id].name, name, DES_MAX_NAME - 1);
    cfg->stages[stage_id].name[DES_MAX_NAME - 1] = '\0';
}

void DesConfig_setArrivalName(DesSimConfig *cfg, int arrival_id, const char *name) {
    if (arrival_id < 0 || arrival_id >= cfg->num_arrivals || !name) return;
    strncpy(cfg->arrivals[arrival_id].name, name, DES_MAX_NAME - 1);
    cfg->arrivals[arrival_id].name[DES_MAX_NAME - 1] = '\0';
}

void DesConfig_setArrivalCount(DesSimConfig *cfg, int arrival_id, int count) {
    if (arrival_id < 0 || arrival_id >= cfg->num_arrivals) return;
    if (count < 1) count = 1;
    cfg->arrivals[arrival_id].entity_count = count;
}

/* --- Remove operations --- */

int DesConfig_removeResource(DesSimConfig *cfg, int resource_id) {
    if (!cfg || resource_id < 0 || resource_id >= cfg->num_resources) return DES_INVALID_ID;

    /* Clear any stages referencing this resource */
    for (int i = 0; i < cfg->num_stages; i++) {
        if (cfg->stages[i].resource_type_id == resource_id) {
            cfg->stages[i].resource_type_id = DES_INVALID_ID;
        } else if (cfg->stages[i].resource_type_id > resource_id) {
            cfg->stages[i].resource_type_id--;
        }
    }

    /* Shift resources down */
    for (int i = resource_id; i < cfg->num_resources - 1; i++) {
        cfg->resources[i] = cfg->resources[i + 1];
    }
    cfg->num_resources--;
    memset(&cfg->resources[cfg->num_resources], 0, sizeof(DesResourceDef));
    return DES_OK;
}

int DesConfig_removeStage(DesSimConfig *cfg, int stage_id) {
    if (!cfg || stage_id < 0 || stage_id >= cfg->num_stages) return DES_INVALID_ID;

    /* Fix arrivals referencing this stage */
    for (int i = 0; i < cfg->num_arrivals; i++) {
        if (cfg->arrivals[i].entry_stage_id == stage_id) {
            cfg->arrivals[i].entry_stage_id = DES_INVALID_ID;
        } else if (cfg->arrivals[i].entry_stage_id > stage_id) {
            cfg->arrivals[i].entry_stage_id--;
        }
    }

    /* Fix outcomes in all stages referencing this stage */
    for (int i = 0; i < cfg->num_stages; i++) {
        for (int j = 0; j < cfg->stages[i].num_outcomes; j++) {
            int ns = cfg->stages[i].outcomes[j].next_stage_id;
            if (ns == stage_id) {
                cfg->stages[i].outcomes[j].next_stage_id = DES_INVALID_ID;
            } else if (ns > stage_id) {
                cfg->stages[i].outcomes[j].next_stage_id--;
            }
        }
    }

    /* Shift stages down */
    for (int i = stage_id; i < cfg->num_stages - 1; i++) {
        cfg->stages[i] = cfg->stages[i + 1];
    }
    cfg->num_stages--;
    memset(&cfg->stages[cfg->num_stages], 0, sizeof(DesStageDef));
    return DES_OK;
}

int DesConfig_removeArrival(DesSimConfig *cfg, int arrival_id) {
    if (!cfg || arrival_id < 0 || arrival_id >= cfg->num_arrivals) return DES_INVALID_ID;

    for (int i = arrival_id; i < cfg->num_arrivals - 1; i++) {
        cfg->arrivals[i] = cfg->arrivals[i + 1];
    }
    cfg->num_arrivals--;
    memset(&cfg->arrivals[cfg->num_arrivals], 0, sizeof(DesEntityArrival));
    return DES_OK;
}
