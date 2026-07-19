#include "des/des_stats.h"
#include "des/des_engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

void DesStats_init(DesStatsCollector *stats) {
    stats->entity_record_capacity = 4096;
    stats->entity_records = (DesEntityRecord *)calloc((size_t)stats->entity_record_capacity,
                                                      sizeof(DesEntityRecord));
    stats->num_entity_records = 0;
    stats->resource_record_capacity = 4096;
    stats->resource_records = (DesResourceRecord *)calloc((size_t)stats->resource_record_capacity,
                                                          sizeof(DesResourceRecord));
    stats->num_resource_records = 0;
    stats->transition_record_capacity = 4096;
    stats->transition_records = (DesTransitionRecord *)calloc(
        (size_t)stats->transition_record_capacity, sizeof(DesTransitionRecord));
    stats->num_transition_records = 0;
}

void DesStats_destroy(DesStatsCollector *stats) {
    free(stats->entity_records);
    free(stats->resource_records);
    free(stats->transition_records);
    stats->entity_records = NULL;
    stats->resource_records = NULL;
    stats->transition_records = NULL;
    stats->num_entity_records = 0;
    stats->num_resource_records = 0;
    stats->num_transition_records = 0;
}

void DesStats_recordEvent(DesEngine *engine, const DesEvent *event) {
    (void)engine;
    (void)event;
}

void DesStats_recordTransition(DesEngine *engine,
                               const DesTransitionRecord *record) {
    if (!engine || !record) return;
    DesStatsCollector *stats = &engine->stats;
    if (stats->num_transition_records >= stats->transition_record_capacity) {
        int new_capacity = stats->transition_record_capacity * 2;
        DesTransitionRecord *records = (DesTransitionRecord *)realloc(
            stats->transition_records,
            (size_t)new_capacity * sizeof(DesTransitionRecord));
        if (!records) {
            engine->error = true;
            engine->last_error = DES_ERR_OUT_OF_MEMORY;
            return;
        }
        stats->transition_records = records;
        stats->transition_record_capacity = new_capacity;
    }
    stats->transition_records[stats->num_transition_records++] = *record;
}

static void writeJsonString(FILE *file, const char *value) {
    fputc('"', file);
    for (const unsigned char *p = (const unsigned char *)value; p && *p; p++) {
        switch (*p) {
            case '"': fputs("\\\"", file); break;
            case '\\': fputs("\\\\", file); break;
            case '\b': fputs("\\b", file); break;
            case '\f': fputs("\\f", file); break;
            case '\n': fputs("\\n", file); break;
            case '\r': fputs("\\r", file); break;
            case '\t': fputs("\\t", file); break;
            default:
                if (*p < 0x20) fprintf(file, "\\u%04x", *p);
                else fputc(*p, file);
        }
    }
    fputc('"', file);
}

static const char *actionName(DesActionType action) {
    switch (action) {
        case DES_ACTION_NONE: return "none";
        case DES_ACTION_ACQUIRE_AND_PROCESS: return "acquire_and_process";
        case DES_ACTION_RELEASE_AND_DISPATCH: return "release_and_dispatch";
        case DES_ACTION_RELEASE_AND_RETRY: return "release_and_retry";
        case DES_ACTION_WAIT_RETRY: return "wait_retry";
        case DES_ACTION_ENTITY_ENTER: return "entity_enter";
        case DES_ACTION_ENTITY_EXIT: return "entity_exit";
        case DES_ACTION_CUSTOM: return "custom";
    }
    return "none";
}

DesErrorCode DesStats_exportReplayJson(const DesEngine *engine,
                                       const char *filepath) {
    if (!engine || !filepath || filepath[0] == '\0') return DES_ERR_NULL_POINTER;
    FILE *file = fopen(filepath, "wb");
    if (!file) return DES_ERR_FILE_IO;
    const DesSimConfig *cfg = engine->config;

    fputs("{\n  \"replay_version\": 1,\n  \"scenario\": { \"name\": ", file);
    writeJsonString(file, cfg->name);
    fprintf(file, ", \"seed\": %u, \"end_time\": %d, \"events_processed\": %d, "
                  "\"entities\": %d, \"completed\": %d },\n",
            cfg->seed, engine->current_time, engine->events_processed,
            engine->next_entity_id, engine->num_completed_entities);

    fputs("  \"stages\": [\n", file);
    for (int i = 0; i < cfg->num_stages; i++) {
        const DesStageDef *stage = &cfg->stages[i];
        fprintf(file, "    { \"id\": %d, \"name\": ", i);
        writeJsonString(file, stage->name);
        fprintf(file, ", \"initial_state\": %d, \"states\": [", stage->initial_state_index);
        for (int j = 0; j < stage->num_states; j++) {
            if (j) fputs(", ", file);
            writeJsonString(file, stage->state_names[j]);
        }
        fputs("], \"events\": [", file);
        for (int j = 0; j < stage->num_event_types; j++) {
            if (j) fputs(", ", file);
            writeJsonString(file, stage->event_type_names[j]);
        }
        fputs("], \"fsm\": [", file);
        for (int j = 0; j < stage->num_transitions; j++) {
            const DesFsmTransition *transition = &stage->transitions[j];
            if (j) fputs(",", file);
            fprintf(file, "{\"state\":%d,\"event\":%d,\"next_state\":%d,\"action\":",
                    transition->state_index, transition->event_index,
                    transition->next_state_index);
            writeJsonString(file, actionName(transition->action_type));
            fputc('}', file);
        }
        fputs("], \"outcomes\": [", file);
        for (int j = 0; j < stage->num_outcomes; j++) {
            const DesStageOutcome *outcome = &stage->outcomes[j];
            if (j) fputs(",", file);
            fputs("{\"name\":", file); writeJsonString(file, outcome->name);
            fprintf(file, ",\"next_stage\":%d}", outcome->next_stage_id);
        }
        fprintf(file, "] }%s\n", i + 1 < cfg->num_stages ? "," : "");
    }

    fputs("  ],\n  \"resources\": [\n", file);
    for (int i = 0; i < cfg->num_resources; i++) {
        fprintf(file, "    { \"id\": %d, \"name\": ", i);
        writeJsonString(file, cfg->resources[i].name);
        fprintf(file, ", \"instances\": %d, \"available_at\": %d }%s\n",
                cfg->resources[i].count, cfg->resources[i].available_at,
                i + 1 < cfg->num_resources ? "," : "");
    }

    fputs("  ],\n  \"entities\": [\n", file);
    for (int i = 0; i < engine->next_entity_id; i++) {
        const DesEntity *entity = &engine->entities[i];
        fprintf(file, "    { \"id\": %d, \"entry_time\": %d, \"completion_time\": %d }%s\n",
                entity->id, entity->entry_time, entity->completion_time,
                i + 1 < engine->next_entity_id ? "," : "");
    }

    fputs("  ],\n  \"visits\": [\n", file);
    for (int i = 0; i < engine->stats.num_entity_records; i++) {
        const DesEntityRecord *record = &engine->stats.entity_records[i];
        fprintf(file, "    { \"entity\": %d, \"stage\": %d, \"enter\": %d, "
                      "\"exit\": %d, \"outcome\": %d }%s\n",
                record->entity_id, record->stage_id, record->enter_time,
                record->exit_time, record->outcome_id,
                i + 1 < engine->stats.num_entity_records ? "," : "");
    }

    fputs("  ],\n  \"resource_timeline\": [\n", file);
    for (int i = 0; i < engine->stats.num_resource_records; i++) {
        const DesResourceRecord *record = &engine->stats.resource_records[i];
        fprintf(file, "    { \"time\": %d, \"resource\": %d, \"instance\": %d, "
                      "\"busy\": %s, \"entity\": %d }%s\n",
                record->time, record->resource_type_id, record->instance_id,
                record->state ? "true" : "false", record->entity_id,
                i + 1 < engine->stats.num_resource_records ? "," : "");
    }

    fputs("  ],\n  \"transitions\": [\n", file);
    for (int i = 0; i < engine->stats.num_transition_records; i++) {
        const DesTransitionRecord *record = &engine->stats.transition_records[i];
        fprintf(file, "    { \"time\": %d, \"event_id\": %d, \"entity\": %d, "
                      "\"stage\": %d, \"event\": %d, \"from_state\": %d, "
                      "\"to_state\": %d, \"action\": ",
                record->time, record->event_id, record->entity_id,
                record->stage_id, record->event_type, record->from_state,
                record->to_state);
        writeJsonString(file, actionName(record->action_type));
        fprintf(file, ", \"accepted\": %s, \"resource\": %d, "
                      "\"instance\": %d, \"outcome\": %d }%s\n",
                record->accepted ? "true" : "false",
                record->resource_type_id, record->resource_instance_id,
                record->outcome_id,
                i + 1 < engine->stats.num_transition_records ? "," : "");
    }
    fputs("  ]\n}\n", file);

    if (fclose(file) != 0) return DES_ERR_FILE_IO;
    return DES_OK;
}

void DesStats_recordEntityTransition(DesEngine *engine, int entity_id,
                                     int stage_id, int enter_time, int exit_time,
                                     int outcome_id) {
    DesStatsCollector *stats = &engine->stats;
    if (stats->num_entity_records >= stats->entity_record_capacity) {
        int new_capacity = stats->entity_record_capacity * 2;
        DesEntityRecord *records = (DesEntityRecord *)realloc(
            stats->entity_records, (size_t)new_capacity * sizeof(DesEntityRecord));
        if (!records) {
            engine->error = true;
            engine->last_error = DES_ERR_OUT_OF_MEMORY;
            return;
        }
        stats->entity_records = records;
        stats->entity_record_capacity = new_capacity;
    }
    DesEntityRecord *r = &stats->entity_records[stats->num_entity_records++];
    r->entity_id = entity_id;
    r->stage_id = stage_id;
    r->enter_time = enter_time;
    r->exit_time = exit_time;
    r->outcome_id = outcome_id;
}

void DesStats_recordResourceState(DesEngine *engine, int time,
                                  int resource_type_id, int instance_id,
                                  int state, int entity_id) {
    DesStatsCollector *stats = &engine->stats;
    if (stats->num_resource_records >= stats->resource_record_capacity) {
        int new_capacity = stats->resource_record_capacity * 2;
        DesResourceRecord *records = (DesResourceRecord *)realloc(
            stats->resource_records, (size_t)new_capacity * sizeof(DesResourceRecord));
        if (!records) {
            engine->error = true;
            engine->last_error = DES_ERR_OUT_OF_MEMORY;
            return;
        }
        stats->resource_records = records;
        stats->resource_record_capacity = new_capacity;
    }
    DesResourceRecord *r = &stats->resource_records[stats->num_resource_records++];
    r->time = time;
    r->resource_type_id = resource_type_id;
    r->instance_id = instance_id;
    r->state = state;
    r->entity_id = entity_id;
}

static int ensure_dir(const char *path) {
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

void DesStats_generateReport(const DesEngine *engine) {
    const DesStatsConfig *sc = &engine->config->stats;
    if (!sc->record_events && !sc->record_entity_flow && !sc->record_resource_util) return;

    ensure_dir(sc->output_dir);

    char filepath[512];

    if (sc->record_entity_flow) {
        snprintf(filepath, sizeof(filepath), "%s/entity_flow.csv", sc->output_dir);
        FILE *f = fopen(filepath, "w");
        if (f) {
            fprintf(f, "EntityID;StageID;EnterTime;ExitTime;Duration;OutcomeID\n");
            for (int i = 0; i < engine->stats.num_entity_records; i++) {
                DesEntityRecord *r = &engine->stats.entity_records[i];
                int duration = (r->exit_time > 0) ? r->exit_time - r->enter_time : -1;
                fprintf(f, "%d;%d;%d;%d;%d;%d\n",
                        r->entity_id, r->stage_id, r->enter_time,
                        r->exit_time, duration, r->outcome_id);
            }
            fclose(f);
        }
    }

    if (sc->record_resource_util) {
        snprintf(filepath, sizeof(filepath), "%s/resource_util.csv", sc->output_dir);
        FILE *f = fopen(filepath, "w");
        if (f) {
            fprintf(f, "Time;ResourceTypeID;InstanceID;State;EntityID\n");
            for (int i = 0; i < engine->stats.num_resource_records; i++) {
                DesResourceRecord *r = &engine->stats.resource_records[i];
                fprintf(f, "%d;%d;%d;%d;%d\n",
                        r->time, r->resource_type_id, r->instance_id,
                        r->state, r->entity_id);
            }
            fclose(f);
        }
    }

    int total_entities = engine->next_entity_id;
    int completed = 0;
    long long total_time = 0;
    int max_time = 0;
    int min_time = INT32_MAX;

    for (int i = 0; i < total_entities; i++) {
        const DesEntity *e = &engine->entities[i];
        if (e->completion_time > 0) {
            completed++;
            int dur = e->completion_time - e->entry_time;
            total_time += dur;
            if (dur > max_time) max_time = dur;
            if (dur < min_time) min_time = dur;
        }
    }

    printf("\n=== Simulation Statistics ===\n");
    printf("Total entities:   %d\n", total_entities);
    printf("Completed:        %d\n", completed);
    printf("Still active:     %d\n", engine->num_active_entities);
    if (completed > 0) {
        printf("Avg flow time:    %lld\n", total_time / completed);
        printf("Min flow time:    %d\n", min_time);
        printf("Max flow time:    %d\n", max_time);
    }
    printf("Simulation time:  %d\n", engine->current_time);
    printf("Events processed: %d\n", engine->events_processed);
    printf("============================\n\n");
}

/* ─── Rich console report ─── */

static const char *bar_chart(int value, int max_value, int width) {
    static char buf[128];
    int filled = 0;
    if (max_value > 0) filled = (int)((double)value / max_value * width);
    if (filled > width) filled = width;
    if (filled < 0) filled = 0;
    int i = 0;
    for (; i < filled; i++) buf[i] = '#';
    for (; i < width; i++) buf[i] = '.';
    buf[i] = '\0';
    return buf;
}

static const char *pct_bar(double pct, int width) {
    static char buf[128];
    int filled = (int)(pct / 100.0 * width);
    if (filled > width) filled = width;
    if (filled < 0) filled = 0;
    int i = 0;
    for (; i < filled; i++) buf[i] = '#';
    for (; i < width; i++) buf[i] = '.';
    buf[i] = '\0';
    return buf;
}

void DesStats_printConfigSummary(const DesEngine *engine) {
    const DesSimConfig *cfg = engine->config;

    printf("\n");
    printf("Configuration\n");
    printf("=============\n\n");

    printf("  Resources\n");
    printf("  ----------\n");
    for (int i = 0; i < cfg->num_resources; i++) {
        const DesResourceDef *r = &cfg->resources[i];
        printf("    %-24s  %2d instances", r->name, r->count);
        if (r->available_at > 0) printf("  (available at t=%d)", r->available_at);
        printf("\n");
    }

    printf("\n  Stages\n");
    printf("  ------\n");
    for (int i = 0; i < cfg->num_stages; i++) {
        const DesStageDef *s = &cfg->stages[i];
        printf("    [%d] %-24s", i, s->name);
        if (s->resource_type_id >= 0)
            printf("  resource: %s", cfg->resources[s->resource_type_id].name);
        printf("\n");

        printf("        States:    ");
        for (int j = 0; j < s->num_states; j++) {
            printf("%s%s", j > 0 ? ", " : "", s->state_names[j]);
        }
        printf("\n");

        printf("        Events:    ");
        for (int j = 0; j < s->num_event_types; j++) {
            printf("%s%s", j > 0 ? ", " : "", s->event_type_names[j]);
        }
        printf("\n");

        printf("        Proc time: ");
        switch (s->processing_time.type) {
            case DES_DIST_FIXED:      printf("fixed %g", s->processing_time.param1); break;
            case DES_DIST_UNIFORM:    printf("uniform [%g, %g]", s->processing_time.param1, s->processing_time.param2); break;
            case DES_DIST_EXPONENTIAL: printf("exponential (lambda=%g)", s->processing_time.param1); break;
            case DES_DIST_NORMAL:     printf("normal (mean=%g, std=%g)", s->processing_time.param1, s->processing_time.param2); break;
        }
        printf("\n");

        printf("        FSM:       %d transitions\n", s->num_transitions);

        if (s->num_outcomes > 0) {
            printf("        Outcomes:  ");
            for (int j = 0; j < s->num_outcomes; j++) {
                const DesStageOutcome *o = &s->outcomes[j];
                if (o->next_stage_id >= 0)
                    printf("%s[%.0f%% -> %s]", j > 0 ? ", " : "", o->probability * 100,
                           cfg->stages[o->next_stage_id].name);
                else
                    printf("%s[%.0f%% -> EXIT]", j > 0 ? ", " : "", o->probability * 100);
            }
            printf("\n");
        }
        printf("\n");
    }

    printf("  Arrivals\n");
    printf("  --------\n");
    for (int i = 0; i < cfg->num_arrivals; i++) {
        const DesEntityArrival *a = &cfg->arrivals[i];
        printf("    %-24s  %4d entities -> %s", a->name, a->entity_count,
               cfg->stages[a->entry_stage_id].name);
        if (a->start_time > 0) printf("  (start: t=%d)", a->start_time);
        if (a->priority > 0) printf("  (priority: %d)", a->priority);
        printf("\n");
        printf("        Inter-arrival: ");
        switch (a->inter_arrival.type) {
            case DES_DIST_FIXED:      printf("fixed %g", a->inter_arrival.param1); break;
            case DES_DIST_UNIFORM:    printf("uniform [%g, %g]", a->inter_arrival.param1, a->inter_arrival.param2); break;
            case DES_DIST_EXPONENTIAL: printf("exponential (lambda=%g)", a->inter_arrival.param1); break;
            case DES_DIST_NORMAL:     printf("normal (mean=%g, std=%g)", a->inter_arrival.param1, a->inter_arrival.param2); break;
        }
        printf("\n");
    }

    printf("\n  Limits: max_time=%d, max_events=%d, seed=%u\n",
           cfg->max_time, cfg->max_events, cfg->seed);
}

void DesStats_printSummary(const DesEngine *engine) {
    int total_entities = engine->next_entity_id;
    int completed = engine->num_completed_entities;
    long long total_flow = 0;
    int min_flow = INT32_MAX;
    int max_flow = 0;
    int flow_histogram[11] = {0};

    for (int i = 0; i < total_entities; i++) {
        const DesEntity *e = &engine->entities[i];
        if (e->completion_time > 0) {
            int dur = e->completion_time - e->entry_time;
            total_flow += dur;
            if (dur < min_flow) min_flow = dur;
            if (dur > max_flow) max_flow = dur;

            int bucket = 10;
            if (completed > 0) {
                double pct = (double)dur / (engine->current_time > 0 ? engine->current_time : 1) * 100;
                bucket = (int)(pct / 10);
                if (bucket > 10) bucket = 10;
            }
            flow_histogram[bucket]++;
        }
    }

    printf("\n");
    printf("Simulation Results\n");
    printf("==================\n\n");

    /* ── Overall ── */
    printf("  Entities:       %d created, %d completed, %d still active\n",
           total_entities, completed, engine->num_active_entities);
    printf("  Simulation:     %d time units, %d events processed\n",
           engine->current_time, engine->events_processed);
    if (completed > 0) {
        printf("  Flow time:      avg=%lld, min=%d, max=%d\n",
               total_flow / completed, min_flow, max_flow);
    }
    printf("\n");

    /* ── Per-stage breakdown ── */
    printf("  Stage Breakdown\n");
    printf("  ----------------\n");
    printf("  %-24s %8s %10s %10s %10s\n",
           "Stage", "Count", "Avg Time", "Min Time", "Max Time");
    printf("  %-24s %8s %10s %10s %10s\n",
           "-----", "-----", "--------", "--------", "--------");

    for (int s = 0; s < engine->num_stages; s++) {
        long long stage_total = 0;
        int stage_count = 0;
        int stage_min = INT32_MAX;
        int stage_max = 0;

        for (int i = 0; i < engine->stats.num_entity_records; i++) {
            DesEntityRecord *r = &engine->stats.entity_records[i];
            if (r->stage_id == s && r->exit_time > 0) {
                int dur = r->exit_time - r->enter_time;
                stage_total += dur;
                stage_count++;
                if (dur < stage_min) stage_min = dur;
                if (dur > stage_max) stage_max = dur;
            }
        }

        if (stage_count > 0) {
            printf("  %-24s %8d %10lld %10d %10d  %s\n",
                   engine->stages[s].name, stage_count,
                   stage_total / stage_count, stage_min, stage_max,
                   bar_chart(stage_count, completed, 20));
        } else {
            printf("  %-24s %8s %10s %10s %10s\n",
                   engine->stages[s].name, "-", "-", "-", "-");
        }
    }
    printf("\n");

    /* ── Per-resource utilization ── */
    if (engine->stats.num_resource_records > 0) {
        printf("  Resource Utilization\n");
        printf("  ---------------------\n");
        printf("  %-24s %6s %8s\n", "Resource", "Count", "Util%");
        printf("  %-24s %6s %8s\n", "--------", "-----", "-----");

        for (int r = 0; r < engine->num_resource_types; r++) {
            const DesResourceType *rt = &engine->resource_types[r];
            long long busy_time = 0;

            for (int inst = 0; inst < rt->instance_count; inst++) {
                int last_time = 0;
                int was_busy = 0;

                for (int i = 0; i < engine->stats.num_resource_records; i++) {
                    DesResourceRecord *rec = &engine->stats.resource_records[i];
                    if (rec->resource_type_id != r || rec->instance_id != inst) continue;

                    if (was_busy) {
                        busy_time += rec->time - last_time;
                    }
                    last_time = rec->time;
                    was_busy = rec->state;
                }
                if (was_busy) {
                    busy_time += engine->current_time - last_time;
                }
            }

            double pct = 0;
            if (engine->current_time > 0 && rt->count > 0)
                pct = (double)busy_time / (engine->current_time * rt->count) * 100.0;

            printf("  %-24s %6d %7.1f%%  %s\n",
                   rt->name, rt->count, pct, pct_bar(pct, 20));
        }
        printf("\n");
    }

    /* ── Per-outcome breakdown ── */
    printf("  Outcome Distribution\n");
    printf("  ---------------------\n");

    for (int s = 0; s < engine->num_stages; s++) {
        const DesStage *stage = &engine->stages[s];
        if (stage->num_outcomes == 0) continue;

        printf("  %s:\n", stage->name);

        int outcome_counts[DES_MAX_OUTCOMES] = {0};
        int stage_total = 0;
        for (int i = 0; i < engine->stats.num_entity_records; i++) {
            DesEntityRecord *r = &engine->stats.entity_records[i];
            if (r->stage_id == s && r->outcome_id >= 0 && r->outcome_id < stage->num_outcomes) {
                outcome_counts[r->outcome_id]++;
                stage_total++;
            }
        }

        for (int j = 0; j < stage->num_outcomes; j++) {
            const DesStageOutcome *o = &stage->outcomes[j];
            double pct = stage_total > 0 ? (double)outcome_counts[j] / stage_total * 100.0 : 0;
            const char *dest = o->next_stage_id >= 0
                ? engine->stages[o->next_stage_id].name
                : "EXIT";
            printf("    %-20s %4d (%5.1f%%)  %s -> %s\n",
                   o->name, outcome_counts[j], pct,
                   bar_chart(outcome_counts[j], stage_total, 15), dest);
        }
    }
    printf("\n");

    /* ── Flow time histogram ── */
    if (completed > 0) {
        printf("  Flow Time Distribution\n");
        printf("  -----------------------\n");
        int max_bucket = 0;
        for (int i = 0; i <= 10; i++) {
            if (flow_histogram[i] > max_bucket) max_bucket = flow_histogram[i];
        }
        for (int i = 0; i <= 10; i++) {
            int lo = i * 10;
            int hi = (i < 10) ? (i + 1) * 10 : 100;
            double pct = (double)flow_histogram[i] / completed * 100.0;
            printf("    %3d-%3d%%: %4d  %s  (%.1f%%)\n",
                   lo, hi, flow_histogram[i],
                   bar_chart(flow_histogram[i], max_bucket, 25), pct);
        }
        printf("\n");
    }

    /* ── Pipeline view ── */
    if (engine->num_stages > 1) {
        printf("  Pipeline\n");
        printf("  ---------\n");
        printf("  ");
        for (int s = 0; s < engine->num_stages; s++) {
            printf("[%s]", engine->stages[s].name);
            if (s < engine->num_stages - 1) printf(" --> ");
        }
        printf("\n\n");
    }
}
