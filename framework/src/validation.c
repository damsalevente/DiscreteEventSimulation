#include "des/des_config.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void addError(DesValidationResult *result, const char *fmt, ...) {
    if (!result || result->num_errors >= DES_MAX_VALIDATION_ERRORS) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(result->errors[result->num_errors], DES_MAX_ERROR_MESSAGE, fmt, args);
    va_end(args);
    result->num_errors++;
}

static bool validDistribution(const DesDistribution *dist) {
    if (!dist || dist->type < DES_DIST_FIXED || dist->type > DES_DIST_NORMAL) return false;
    if (!isfinite(dist->param1) || !isfinite(dist->param2)) return false;
    switch (dist->type) {
        case DES_DIST_FIXED:       return dist->param1 >= 0.0;
        case DES_DIST_UNIFORM:     return dist->param1 >= 0.0 && dist->param2 >= dist->param1;
        case DES_DIST_EXPONENTIAL: return dist->param1 > 0.0;
        case DES_DIST_NORMAL:      return dist->param1 >= 0.0 && dist->param2 >= 0.0;
    }
    return false;
}

bool DesConfig_validate(const DesSimConfig *cfg, DesValidationResult *result) {
    DesValidationResult local;
    if (!result) result = &local;
    memset(result, 0, sizeof(*result));

    if (!cfg) {
        addError(result, "configuration is null");
        return false;
    }
    if (cfg->max_time <= 0) addError(result, "simulation.max_time must be positive");
    if (cfg->max_events <= 0) addError(result, "simulation.max_events must be positive");
    if (cfg->entity_capacity < 0) addError(result, "simulation.entity_capacity cannot be negative");
    if (cfg->num_resources < 0 || cfg->num_resources > DES_MAX_RESOURCES)
        addError(result, "resource count exceeds framework limits");
    if (cfg->num_stages <= 0 || cfg->num_stages > DES_MAX_STAGES)
        addError(result, "at least one stage is required");
    if (cfg->num_arrivals < 0 || cfg->num_arrivals > DES_MAX_ARRIVALS)
        addError(result, "arrival stream count exceeds framework limits");

    for (int i = 0; i < cfg->num_resources && i < DES_MAX_RESOURCES; i++) {
        const DesResourceDef *resource = &cfg->resources[i];
        if (resource->name[0] == '\0') addError(result, "resource[%d] has no name", i);
        if (resource->count <= 0) addError(result, "resource '%s' must have a positive count", resource->name);
        if (resource->available_at < 0) addError(result, "resource '%s' has a negative available_at", resource->name);
        for (int j = i + 1; j < cfg->num_resources; j++) {
            if (strcmp(resource->name, cfg->resources[j].name) == 0)
                addError(result, "duplicate resource name '%s'", resource->name);
        }
    }

    for (int i = 0; i < cfg->num_stages && i < DES_MAX_STAGES; i++) {
        const DesStageDef *stage = &cfg->stages[i];
        if (stage->name[0] == '\0') addError(result, "stage[%d] has no name", i);
        for (int j = i + 1; j < cfg->num_stages; j++) {
            if (strcmp(stage->name, cfg->stages[j].name) == 0)
                addError(result, "duplicate stage name '%s'", stage->name);
        }
        if (stage->resource_type_id != DES_INVALID_ID &&
            (stage->resource_type_id < 0 || stage->resource_type_id >= cfg->num_resources))
            addError(result, "stage '%s' references an invalid resource", stage->name);
        if (stage->num_states <= 0 || stage->num_states > DES_MAX_STATES)
            addError(result, "stage '%s' must define at least one state", stage->name);
        if (stage->initial_state_index < 0 || stage->initial_state_index >= stage->num_states)
            addError(result, "stage '%s' has an invalid initial state", stage->name);
        if (stage->num_event_types <= 0 || stage->num_event_types > DES_MAX_EVENT_TYPES)
            addError(result, "stage '%s' must define at least one event type", stage->name);
        if (!validDistribution(&stage->processing_time))
            addError(result, "stage '%s' has an invalid processing-time distribution", stage->name);
        if (stage->num_transitions <= 0)
            addError(result, "stage '%s' must define at least one transition", stage->name);
        for (int s = 0; s < stage->num_states; s++) {
            for (int u = s + 1; u < stage->num_states; u++) {
                if (strcmp(stage->state_names[s], stage->state_names[u]) == 0)
                    addError(result, "stage '%s' has duplicate state '%s'", stage->name, stage->state_names[s]);
            }
        }
        for (int e = 0; e < stage->num_event_types; e++) {
            for (int u = e + 1; u < stage->num_event_types; u++) {
                if (strcmp(stage->event_type_names[e], stage->event_type_names[u]) == 0)
                    addError(result, "stage '%s' has duplicate event '%s'", stage->name, stage->event_type_names[e]);
            }
        }

        for (int t = 0; t < stage->num_transitions; t++) {
            const DesFsmTransition *transition = &stage->transitions[t];
            if (transition->state_index < 0 || transition->state_index >= stage->num_states ||
                transition->next_state_index < 0 || transition->next_state_index >= stage->num_states ||
                transition->event_index < 0 || transition->event_index >= stage->num_event_types)
                addError(result, "stage '%s' transition[%d] contains an invalid state or event", stage->name, t);
            if (transition->action_type < DES_ACTION_NONE || transition->action_type > DES_ACTION_CUSTOM)
                addError(result, "stage '%s' transition[%d] has an unknown action", stage->name, t);
            for (int u = t + 1; u < stage->num_transitions; u++) {
                if (transition->state_index == stage->transitions[u].state_index &&
                    transition->event_index == stage->transitions[u].event_index)
                    addError(result, "stage '%s' has duplicate transitions for state %d/event %d",
                             stage->name, transition->state_index, transition->event_index);
            }
        }

        double probability_sum = 0.0;
        for (int o = 0; o < stage->num_outcomes; o++) {
            const DesStageOutcome *outcome = &stage->outcomes[o];
            if (!isfinite(outcome->probability) || outcome->probability < 0.0 || outcome->probability > 1.0)
                addError(result, "stage '%s' outcome '%s' has an invalid probability", stage->name, outcome->name);
            probability_sum += outcome->probability;
            if (outcome->next_stage_id != DES_INVALID_ID) {
                if (outcome->next_stage_id < 0 || outcome->next_stage_id >= cfg->num_stages) {
                    addError(result, "stage '%s' outcome '%s' references an invalid stage", stage->name, outcome->name);
                } else if (outcome->next_event_index < 0 ||
                           outcome->next_event_index >= cfg->stages[outcome->next_stage_id].num_event_types) {
                    addError(result, "stage '%s' outcome '%s' references an invalid event", stage->name, outcome->name);
                }
            }
        }
        if (stage->num_outcomes > 0 && fabs(probability_sum - 1.0) > 1e-6)
            addError(result, "stage '%s' outcome probabilities sum to %.6g instead of 1", stage->name, probability_sum);
    }

    long long total_entities = 0;
    for (int i = 0; i < cfg->num_arrivals && i < DES_MAX_ARRIVALS; i++) {
        const DesEntityArrival *arrival = &cfg->arrivals[i];
        if (arrival->name[0] == '\0') addError(result, "arrival[%d] has no name", i);
        if (arrival->entity_count <= 0) addError(result, "arrival '%s' must have a positive count", arrival->name);
        if (arrival->entry_stage_id < 0 || arrival->entry_stage_id >= cfg->num_stages)
            addError(result, "arrival '%s' references an invalid entry stage", arrival->name);
        if (arrival->start_time < 0) addError(result, "arrival '%s' has a negative start time", arrival->name);
        if (!validDistribution(&arrival->inter_arrival))
            addError(result, "arrival '%s' has an invalid inter-arrival distribution", arrival->name);
        total_entities += arrival->entity_count;
    }
    if (cfg->entity_capacity > 0 && total_entities > cfg->entity_capacity)
        addError(result, "entity_capacity %d is smaller than the configured %lld entities",
                 cfg->entity_capacity, total_entities);

    return result->num_errors == 0;
}
