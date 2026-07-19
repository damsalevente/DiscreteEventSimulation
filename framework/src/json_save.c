#include "des/des_config.h"
#include <stdio.h>
#include <string.h>

static void writeEscaped(FILE *file, const char *value) {
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

static const char *distributionName(DesDistType type) {
    switch (type) {
        case DES_DIST_FIXED: return "fixed";
        case DES_DIST_UNIFORM: return "uniform";
        case DES_DIST_EXPONENTIAL: return "exponential";
        case DES_DIST_NORMAL: return "normal";
    }
    return "fixed";
}

static const char *actionName(DesActionType type) {
    switch (type) {
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

static void writeDistribution(FILE *file, const DesDistribution *dist) {
    fprintf(file, "{ \"distribution\": \"%s\", \"param1\": %.17g, \"param2\": %.17g }",
            distributionName(dist->type), dist->param1, dist->param2);
}

DesErrorCode DesConfig_saveJson(const DesSimConfig *cfg, const char *filepath) {
    if (!cfg || !filepath || filepath[0] == '\0') return DES_ERR_NULL_POINTER;

    DesValidationResult validation;
    if (!DesConfig_validate(cfg, &validation)) return DES_ERR_CONFIG;

    char temporary[1024];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", filepath) >= (int)sizeof(temporary))
        return DES_ERR_FILE_IO;
    FILE *file = fopen(temporary, "wb");
    if (!file) return DES_ERR_FILE_IO;

    fputs("{\n  \"format_version\": 1,\n  \"name\": ", file);
    writeEscaped(file, cfg->name);
    fputs(",\n  \"simulation\": {\n", file);
    fprintf(file, "    \"max_time\": %d,\n    \"max_events\": %d,\n    \"entity_capacity\": %d,\n    \"seed\": %u\n  },\n",
            cfg->max_time, cfg->max_events, cfg->entity_capacity, cfg->seed);

    fputs("  \"resources\": [\n", file);
    for (int i = 0; i < cfg->num_resources; i++) {
        fputs("    { \"name\": ", file); writeEscaped(file, cfg->resources[i].name);
        fprintf(file, ", \"count\": %d, \"available_at\": %d }%s\n",
                cfg->resources[i].count, cfg->resources[i].available_at,
                i + 1 < cfg->num_resources ? "," : "");
    }
    fputs("  ],\n  \"stages\": [\n", file);
    for (int i = 0; i < cfg->num_stages; i++) {
        const DesStageDef *stage = &cfg->stages[i];
        fputs("    {\n      \"name\": ", file); writeEscaped(file, stage->name); fputs(",\n", file);
        fputs("      \"mode\": \"manual\",\n", file);
        if (stage->resource_type_id != DES_INVALID_ID) {
            fputs("      \"resource\": ", file);
            writeEscaped(file, cfg->resources[stage->resource_type_id].name);
            fputs(",\n", file);
        }
        fputs("      \"states\": [", file);
        for (int j = 0; j < stage->num_states; j++) {
            if (j) fputs(", ", file);
            writeEscaped(file, stage->state_names[j]);
        }
        fputs("],\n      \"event_types\": [", file);
        for (int j = 0; j < stage->num_event_types; j++) {
            if (j) fputs(", ", file);
            writeEscaped(file, stage->event_type_names[j]);
        }
        fputs("],\n      \"initial_state\": ", file);
        writeEscaped(file, stage->state_names[stage->initial_state_index]);
        fputs(",\n      \"processing_time\": ", file);
        writeDistribution(file, &stage->processing_time);
        fputs(",\n      \"fsm\": [\n", file);
        for (int j = 0; j < stage->num_transitions; j++) {
            const DesFsmTransition *transition = &stage->transitions[j];
            fputs("        { \"state\": ", file); writeEscaped(file, stage->state_names[transition->state_index]);
            fputs(", \"event\": ", file); writeEscaped(file, stage->event_type_names[transition->event_index]);
            fputs(", \"next_state\": ", file); writeEscaped(file, stage->state_names[transition->next_state_index]);
            fputs(", \"action\": ", file); writeEscaped(file, actionName(transition->action_type));
            fprintf(file, " }%s\n", j + 1 < stage->num_transitions ? "," : "");
        }
        fputs("      ],\n      \"outcomes\": [\n", file);
        for (int j = 0; j < stage->num_outcomes; j++) {
            const DesStageOutcome *outcome = &stage->outcomes[j];
            fputs("        { \"name\": ", file); writeEscaped(file, outcome->name);
            fprintf(file, ", \"probability\": %.17g, \"next_stage\": ", outcome->probability);
            if (outcome->next_stage_id == DES_INVALID_ID) {
                fputs("null", file);
            } else {
                writeEscaped(file, cfg->stages[outcome->next_stage_id].name);
                fputs(", \"next_event\": ", file);
                writeEscaped(file, cfg->stages[outcome->next_stage_id].event_type_names[outcome->next_event_index]);
            }
            fprintf(file, " }%s\n", j + 1 < stage->num_outcomes ? "," : "");
        }
        fprintf(file, "      ]\n    }%s\n", i + 1 < cfg->num_stages ? "," : "");
    }

    fputs("  ],\n  \"entity_arrivals\": [\n", file);
    for (int i = 0; i < cfg->num_arrivals; i++) {
        const DesEntityArrival *arrival = &cfg->arrivals[i];
        fputs("    { \"name\": ", file); writeEscaped(file, arrival->name);
        fprintf(file, ", \"count\": %d, \"entry_stage\": ", arrival->entity_count);
        writeEscaped(file, cfg->stages[arrival->entry_stage_id].name);
        fputs(", \"inter_arrival\": ", file); writeDistribution(file, &arrival->inter_arrival);
        fprintf(file, ", \"start_time\": %d, \"priority\": %d }%s\n",
                arrival->start_time, arrival->priority, i + 1 < cfg->num_arrivals ? "," : "");
    }

    fputs("  ],\n  \"statistics\": {\n", file);
    fprintf(file, "    \"record_events\": %s,\n    \"record_entity_flow\": %s,\n    \"record_resource_util\": %s,\n    \"output_dir\": ",
            cfg->stats.record_events ? "true" : "false",
            cfg->stats.record_entity_flow ? "true" : "false",
            cfg->stats.record_resource_util ? "true" : "false");
    writeEscaped(file, cfg->stats.output_dir);
    fputs("\n  }\n}\n", file);

    if (fclose(file) != 0) {
        remove(temporary);
        return DES_ERR_FILE_IO;
    }
    if (rename(temporary, filepath) != 0) {
        remove(temporary);
        return DES_ERR_FILE_IO;
    }
    return DES_OK;
}
