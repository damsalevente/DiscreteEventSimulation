#include "des/des.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    double sum;
    double sum_squared;
} MetricAggregate;

typedef struct {
    int runs;
    int successful_runs;
    long long entities;
    long long completed_total;
    long long active_total;
    long long events;
    MetricAggregate completed;
    MetricAggregate completion_rate;
    MetricAggregate throughput;
    MetricAggregate mean_flow;
    MetricAggregate p95_flow;
    MetricAggregate makespan;
    MetricAggregate utilization;
} ExperimentSummary;

typedef enum {
    OBJECTIVE_NONE = 0,
    OBJECTIVE_THROUGHPUT,
    OBJECTIVE_MEAN_FLOW
} ObjectiveMetric;

typedef struct {
    ObjectiveMetric metric;
    double target;
} Objective;

static void printJsonString(const char *value) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)value; p && *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else if (*p == '\n') fputs("\\n", stdout);
        else if (*p == '\r') fputs("\\r", stdout);
        else if (*p == '\t') fputs("\\t", stdout);
        else if (*p < 0x20) printf("\\u%04x", *p);
        else putchar(*p);
    }
    putchar('"');
}

static void usage(FILE *stream) {
    fprintf(stream,
        "DES Experiment Workbench CLI\n\n"
        "Usage:\n"
        "  desim validate <scenario.json> [--json]\n"
        "  desim run <scenario.json> [--seed N] [--json] [--report]\n"
        "            [--resource NAME=COUNT] [--replay FILE]\n"
        "  desim experiment <scenario.json> [--runs N] [--seed N] [--json]\n"
        "                   [--resource NAME=COUNT] [--objective EXPR]\n"
        "  desim sweep <scenario.json> --resource NAME=MIN:MAX --objective EXPR\n"
        "              [--runs N] [--seed N] [--json]\n\n"
        "Objectives:\n"
        "  throughput>=VALUE   Minimum completed entities per time unit\n"
        "  mean-flow<=VALUE    Maximum mean entity flow time\n");
}

static int findResource(const DesSimConfig *cfg, const char *name) {
    for (int i = 0; i < cfg->num_resources; i++)
        if (strcmp(cfg->resources[i].name, name) == 0) return i;
    return DES_INVALID_ID;
}

static int parseResourceOverride(const char *text, char *name, size_t name_size, int *count) {
    const char *equals = strchr(text, '=');
    if (!equals || equals == text) return 0;
    size_t length = (size_t)(equals - text);
    if (length >= name_size) return 0;
    memcpy(name, text, length);
    name[length] = '\0';
    char *end = NULL;
    long value = strtol(equals + 1, &end, 10);
    if (!end || *end != '\0' || value < 1 || value > 1000000) return 0;
    *count = (int)value;
    return 1;
}

static int parseSweep(const char *text, char *name, size_t name_size,
                      int *minimum, int *maximum) {
    const char *equals = strchr(text, '=');
    const char *colon = equals ? strchr(equals + 1, ':') : NULL;
    if (!equals || !colon || equals == text) return 0;
    size_t length = (size_t)(equals - text);
    if (length >= name_size) return 0;
    memcpy(name, text, length);
    name[length] = '\0';
    char min_buffer[32];
    size_t min_length = (size_t)(colon - equals - 1);
    if (min_length == 0 || min_length >= sizeof(min_buffer)) return 0;
    memcpy(min_buffer, equals + 1, min_length);
    min_buffer[min_length] = '\0';
    char *min_end = NULL;
    char *max_end = NULL;
    long min_value = strtol(min_buffer, &min_end, 10);
    long max_value = strtol(colon + 1, &max_end, 10);
    if (!min_end || *min_end != '\0' || !max_end || *max_end != '\0' ||
        min_value < 1 || max_value < min_value || max_value > 10000) return 0;
    *minimum = (int)min_value;
    *maximum = (int)max_value;
    return 1;
}

static int parseObjective(const char *text, Objective *objective) {
    if (!text || !objective) return 0;
    const char *number = NULL;
    if (strncmp(text, "throughput>=", 12) == 0) {
        objective->metric = OBJECTIVE_THROUGHPUT;
        number = text + 12;
    } else if (strncmp(text, "mean-flow<=", 11) == 0) {
        objective->metric = OBJECTIVE_MEAN_FLOW;
        number = text + 11;
    } else {
        return 0;
    }
    char *end = NULL;
    objective->target = strtod(number, &end);
    return end && end != number && *end == '\0' &&
           isfinite(objective->target) && objective->target >= 0.0;
}

static const char *objectiveName(ObjectiveMetric metric) {
    return metric == OBJECTIVE_THROUGHPUT ? "throughput" : "mean-flow";
}

static const char *objectiveOperator(ObjectiveMetric metric) {
    return metric == OBJECTIVE_THROUGHPUT ? ">=" : "<=";
}

static void addMetric(MetricAggregate *metric, double value) {
    metric->sum += value;
    metric->sum_squared += value * value;
}

static double metricMean(const MetricAggregate *metric, int count) {
    return count > 0 ? metric->sum / count : 0.0;
}

static double metricCi95(const MetricAggregate *metric, int count) {
    if (count < 2) return 0.0;
    double mean = metric->sum / count;
    double variance = (metric->sum_squared - count * mean * mean) / (count - 1.0);
    if (variance < 0.0) variance = 0.0;
    return 1.96 * sqrt(variance / count);
}

static int compareInt(const void *left, const void *right) {
    int a = *(const int *)left;
    int b = *(const int *)right;
    return (a > b) - (a < b);
}

static void flowMetrics(const DesEngine *engine, double *mean, double *p95) {
    int *durations = (int *)malloc((size_t)(engine->next_entity_id > 0 ? engine->next_entity_id : 1) * sizeof(int));
    int count = 0;
    long long total = 0;
    if (!durations) { *mean = 0.0; *p95 = 0.0; return; }
    for (int i = 0; i < engine->next_entity_id; i++) {
        const DesEntity *entity = &engine->entities[i];
        if (!entity->active && entity->completion_time >= entity->entry_time) {
            int duration = entity->completion_time - entity->entry_time;
            durations[count++] = duration;
            total += duration;
        }
    }
    *mean = count > 0 ? (double)total / count : 0.0;
    if (count > 0) {
        qsort(durations, (size_t)count, sizeof(int), compareInt);
        int index = (int)ceil(0.95 * count) - 1;
        if (index < 0) index = 0;
        *p95 = durations[index];
    } else {
        *p95 = 0.0;
    }
    free(durations);
}

static double resourceUtilization(const DesEngine *engine, int resource_filter) {
    double busy_time = 0.0;
    double available_time = 0.0;
    int end_time = engine->current_time;
    for (int resource = 0; resource < engine->num_resource_types; resource++) {
        if (resource_filter != DES_INVALID_ID && resource != resource_filter) continue;
        const DesResourceType *type = &engine->resource_types[resource];
        int available_at = engine->config->resources[resource].available_at;
        int window = end_time > available_at ? end_time - available_at : 0;
        available_time += (double)window * type->instance_count;
        for (int instance = 0; instance < type->instance_count; instance++) {
            int busy_start = DES_INVALID_ID;
            for (int i = 0; i < engine->stats.num_resource_records; i++) {
                const DesResourceRecord *record = &engine->stats.resource_records[i];
                if (record->resource_type_id != resource || record->instance_id != instance) continue;
                if (record->state && busy_start == DES_INVALID_ID) busy_start = record->time;
                if (!record->state && busy_start != DES_INVALID_ID) {
                    busy_time += record->time - busy_start;
                    busy_start = DES_INVALID_ID;
                }
            }
            if (busy_start != DES_INVALID_ID && end_time > busy_start)
                busy_time += end_time - busy_start;
        }
    }
    return available_time > 0.0 ? busy_time / available_time : 0.0;
}

static int configuredEntityCount(const DesSimConfig *config) {
    int count = 0;
    for (int i = 0; i < config->num_arrivals; i++) count += config->arrivals[i].entity_count;
    return count;
}

static void collectRun(const DesEngine *engine, int resource_id,
                       int configured_entities, ExperimentSummary *summary) {
    double mean_flow, p95_flow;
    flowMetrics(engine, &mean_flow, &p95_flow);
    double makespan = engine->current_time;
    double throughput = makespan > 0.0 ? engine->num_completed_entities / makespan : 0.0;
    double completion_rate = configured_entities > 0
        ? (double)engine->num_completed_entities / configured_entities : 0.0;
    double utilization = resourceUtilization(engine, resource_id);

    summary->successful_runs++;
    summary->entities += engine->next_entity_id;
    summary->completed_total += engine->num_completed_entities;
    summary->active_total += engine->num_active_entities;
    summary->events += engine->events_processed;
    addMetric(&summary->completed, engine->num_completed_entities);
    addMetric(&summary->completion_rate, completion_rate);
    addMetric(&summary->throughput, throughput);
    addMetric(&summary->mean_flow, mean_flow);
    addMetric(&summary->p95_flow, p95_flow);
    addMetric(&summary->makespan, makespan);
    addMetric(&summary->utilization, utilization);
}

static DesErrorCode runEngine(DesSimConfig *config, int resource_id,
                              ExperimentSummary *summary, const char *replay_path,
                              bool report) {
    config->stats.record_entity_flow = true;
    config->stats.record_resource_util = true;
    if (replay_path) config->stats.record_events = true;
    DesEngine *engine = DesEngine_create(config);
    if (!engine) return DES_ERR_CONFIG;
    DesErrorCode error = DesEngine_run(engine);
    if (error == DES_OK) {
        collectRun(engine, resource_id, configuredEntityCount(config), summary);
        if (report) DesStats_generateReport(engine);
        if (replay_path) error = DesStats_exportReplayJson(engine, replay_path);
    }
    DesEngine_destroy(engine);
    return error;
}

static DesErrorCode executeRuns(const DesSimConfig *base, int runs,
                                unsigned int seed, int resource_id,
                                ExperimentSummary *summary) {
    memset(summary, 0, sizeof(*summary));
    summary->runs = runs;
    for (int run = 0; run < runs; run++) {
        DesSimConfig *config = (DesSimConfig *)malloc(sizeof(*config));
        if (!config) return DES_ERR_OUT_OF_MEMORY;
        memcpy(config, base, sizeof(*config));
        config->seed = seed + (unsigned int)run;
        DesErrorCode error = runEngine(config, resource_id, summary, NULL, false);
        free(config);
        if (error != DES_OK) return error;
    }
    return DES_OK;
}

static void printMetricJson(const char *name, const MetricAggregate *metric, int count) {
    printf("\"%s\":{\"mean\":%.6f,\"ci95\":%.6f}",
           name, metricMean(metric, count), metricCi95(metric, count));
}

static void printSummary(const ExperimentSummary *summary, bool json) {
    int count = summary->successful_runs;
    if (json) {
        printf("{\"runs\":%d,\"successful_runs\":%d,\"entities\":%lld,"
               "\"completed_total\":%lld,\"active_total\":%lld,\"events_processed\":%lld,"
               "\"metrics\":{", summary->runs, count, summary->entities,
               summary->completed_total, summary->active_total, summary->events);
        printMetricJson("completed", &summary->completed, count); putchar(',');
        printMetricJson("completion_rate", &summary->completion_rate, count); putchar(',');
        printMetricJson("throughput", &summary->throughput, count); putchar(',');
        printMetricJson("mean_flow", &summary->mean_flow, count); putchar(',');
        printMetricJson("p95_flow", &summary->p95_flow, count); putchar(',');
        printMetricJson("makespan", &summary->makespan, count); putchar(',');
        printMetricJson("utilization", &summary->utilization, count);
        fputs("}}", stdout);
    } else {
        printf("runs=%d completed=%.1f throughput=%.4f ± %.4f mean_flow=%.2f ± %.2f "
               "p95_flow=%.2f utilization=%.1f%% makespan=%.2f",
               count, metricMean(&summary->completed, count),
               metricMean(&summary->throughput, count), metricCi95(&summary->throughput, count),
               metricMean(&summary->mean_flow, count), metricCi95(&summary->mean_flow, count),
               metricMean(&summary->p95_flow, count),
               100.0 * metricMean(&summary->utilization, count),
               metricMean(&summary->makespan, count));
    }
}

static bool meetsObjective(const ExperimentSummary *summary, const Objective *objective) {
    int count = summary->successful_runs;
    if (objective->metric == OBJECTIVE_THROUGHPUT)
        return metricMean(&summary->throughput, count) - metricCi95(&summary->throughput, count) >= objective->target;
    if (objective->metric == OBJECTIVE_MEAN_FLOW)
        return metricMean(&summary->mean_flow, count) + metricCi95(&summary->mean_flow, count) <= objective->target;
    return false;
}

static int validationCommand(const char *path, bool json) {
    DesSimConfig *config = DesConfig_loadJson(path);
    if (!config) {
        if (json) { printf("{\"result_version\":2,\"valid\":false,\"error\":"); printJsonString(DesConfig_getLoadError()); printf("}\n"); }
        else fprintf(stderr, "Invalid scenario: %s\n", DesConfig_getLoadError());
        return 2;
    }
    if (json) printf("{\"result_version\":2,\"valid\":true,\"resources\":%d,\"stages\":%d,\"arrivals\":%d}\n",
                     config->num_resources, config->num_stages, config->num_arrivals);
    else printf("Valid scenario: %d resources, %d stages, %d arrival streams\n",
                config->num_resources, config->num_stages, config->num_arrivals);
    DesConfig_destroy(config);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) { usage(stderr); return 1; }
    const char *command = argv[1];
    const char *path = argv[2];
    int runs = strcmp(command, "run") == 0 ? 1 : 30;
    unsigned int seed = 42;
    bool json = false;
    bool report = false;
    const char *resource_argument = NULL;
    const char *replay_path = NULL;
    Objective objective = { OBJECTIVE_NONE, 0.0 };

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) json = true;
        else if (strcmp(argv[i], "--report") == 0) report = true;
        else if (strcmp(argv[i], "--runs") == 0 && i + 1 < argc) runs = atoi(argv[++i]);
        else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) seed = (unsigned int)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--resource") == 0 && i + 1 < argc) resource_argument = argv[++i];
        else if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc) replay_path = argv[++i];
        else if (strcmp(argv[i], "--objective") == 0 && i + 1 < argc) {
            if (!parseObjective(argv[++i], &objective)) {
                fprintf(stderr, "Expected --objective throughput>=VALUE or mean-flow<=VALUE\n");
                return 1;
            }
        } else { fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]); return 1; }
    }
    if (runs < 1 || runs > 100000) { fprintf(stderr, "--runs must be between 1 and 100000\n"); return 1; }
    if (strcmp(command, "validate") == 0) return validationCommand(path, json);

    DesSimConfig *config = DesConfig_loadJson(path);
    if (!config) { fprintf(stderr, "Cannot load scenario: %s\n", DesConfig_getLoadError()); return 2; }

    if (strcmp(command, "run") == 0 || strcmp(command, "experiment") == 0) {
        int resource_id = DES_INVALID_ID;
        if (resource_argument) {
            char name[DES_MAX_NAME];
            int count;
            if (!parseResourceOverride(resource_argument, name, sizeof(name), &count)) {
                fprintf(stderr, "Expected --resource NAME=COUNT\n"); DesConfig_destroy(config); return 1;
            }
            resource_id = findResource(config, name);
            if (resource_id == DES_INVALID_ID) {
                fprintf(stderr, "Unknown resource: %s\n", name); DesConfig_destroy(config); return 2;
            }
            config->resources[resource_id].count = count;
        }
        ExperimentSummary summary;
        memset(&summary, 0, sizeof(summary));
        summary.runs = runs;
        DesErrorCode error;
        if (strcmp(command, "run") == 0) {
            config->seed = seed;
            error = runEngine(config, resource_id, &summary, replay_path, report);
        } else {
            if (replay_path) {
                fprintf(stderr, "--replay is only valid with run\n"); DesConfig_destroy(config); return 1;
            }
            error = executeRuns(config, runs, seed, resource_id, &summary);
        }
        if (error != DES_OK) {
            fprintf(stderr, "Simulation failed: %s\n", DesError_toString(error));
            DesConfig_destroy(config); return 3;
        }
        if (json) { fputs("{\"result_version\":2,\"summary\":", stdout); printSummary(&summary, true); }
        else printSummary(&summary, false);
        if (objective.metric != OBJECTIVE_NONE) {
            bool met = meetsObjective(&summary, &objective);
            if (json) printf(",\"meets_objective\":%s", met ? "true" : "false");
            else printf(" objective=%s", met ? "met" : "not-met");
        }
        if (json) fputs("}\n", stdout); else putchar('\n');
    } else if (strcmp(command, "sweep") == 0) {
        char name[DES_MAX_NAME];
        int minimum, maximum;
        if (!resource_argument || !parseSweep(resource_argument, name, sizeof(name), &minimum, &maximum)) {
            fprintf(stderr, "Sweep requires --resource NAME=MIN:MAX\n"); DesConfig_destroy(config); return 1;
        }
        if (objective.metric == OBJECTIVE_NONE) {
            fprintf(stderr, "Sweep requires --objective throughput>=VALUE or mean-flow<=VALUE\n");
            DesConfig_destroy(config); return 1;
        }
        int resource_id = findResource(config, name);
        if (resource_id == DES_INVALID_ID) {
            fprintf(stderr, "Unknown resource: %s\n", name); DesConfig_destroy(config); return 2;
        }
        int result_count = maximum - minimum + 1;
        ExperimentSummary *summaries = (ExperimentSummary *)calloc((size_t)result_count, sizeof(*summaries));
        bool *meets = (bool *)calloc((size_t)result_count, sizeof(*meets));
        if (!summaries || !meets) {
            free(summaries); free(meets); DesConfig_destroy(config); return 3;
        }
        int recommended = DES_INVALID_ID;
        for (int index = 0; index < result_count; index++) {
            int count = minimum + index;
            config->resources[resource_id].count = count;
            DesErrorCode error = executeRuns(config, runs, seed, resource_id, &summaries[index]);
            if (error != DES_OK) {
                fprintf(stderr, "Sweep failed at count %d: %s\n", count, DesError_toString(error));
                free(summaries); free(meets); DesConfig_destroy(config); return 3;
            }
            meets[index] = meetsObjective(&summaries[index], &objective);
            if (recommended == DES_INVALID_ID && meets[index]) recommended = count;
        }
        if (json) {
            fputs("{\"result_version\":2,\"resource\":", stdout); printJsonString(name);
            printf(",\"objective\":{\"metric\":\"%s\",\"operator\":\"%s\",\"target\":%.6f},"
                   "\"recommended_capacity\":", objectiveName(objective.metric),
                   objectiveOperator(objective.metric), objective.target);
            if (recommended == DES_INVALID_ID) fputs("null", stdout); else printf("%d", recommended);
            fputs(",\"results\":[", stdout);
            for (int index = 0; index < result_count; index++) {
                if (index) putchar(',');
                printf("{\"count\":%d,\"meets_objective\":%s,\"summary\":",
                       minimum + index, meets[index] ? "true" : "false");
                printSummary(&summaries[index], true);
                putchar('}');
            }
            fputs("]}\n", stdout);
        } else {
            for (int index = 0; index < result_count; index++) {
                printf("%s=%d: ", name, minimum + index);
                printSummary(&summaries[index], false);
                printf(" objective=%s\n", meets[index] ? "met" : "not-met");
            }
            if (recommended == DES_INVALID_ID)
                printf("No tested capacity meets %s%s%.4f with 95%% confidence.\n",
                       objectiveName(objective.metric), objectiveOperator(objective.metric), objective.target);
            else
                printf("Recommended capacity: %s=%d (smallest confidence-qualified option).\n",
                       name, recommended);
        }
        free(summaries);
        free(meets);
    } else {
        usage(stderr); DesConfig_destroy(config); return 1;
    }

    DesConfig_destroy(config);
    return 0;
}
