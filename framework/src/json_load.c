#include "des/des_json_load.h"
#include "des/des_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_load_error[256] = {0};

const char *DesConfig_getLoadError(void) {
    return g_load_error;
}

static void set_load_error(const char *fmt, const char *detail) {
    if (detail)
        snprintf(g_load_error, sizeof(g_load_error), "%s: %s", fmt, detail);
    else
        snprintf(g_load_error, sizeof(g_load_error), "%s", fmt);
}

#ifndef CJSON_H
#define CJSON_H
#endif

typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    double valueint;
    double valuedouble;
    char *string;
    char *valuestring;
    int stringlen;
} cJSON;

typedef enum { cJSON_False = 0, cJSON_True, cJSON_NULL, cJSON_Number, cJSON_String,
               cJSON_Array, cJSON_Object } cJSON_Enums;

static cJSON *cJSON_Parse(const char *value);
static cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string);
static cJSON *cJSON_GetArrayItem(const cJSON *array, int index);
static int    cJSON_GetArraySize(const cJSON *array);
static double cJSON_GetNumberValue(const cJSON *item);
static const char *cJSON_GetStringValue(const cJSON *item);
static void   cJSON_Delete(cJSON *c);

#include "json_parser.inc"

static const char *safeString(const cJSON *item, const char *def) {
    if (!item) return def;
    const char *s = cJSON_GetStringValue(item);
    return s ? s : def;
}

static double safeNumber(const cJSON *item, double def) {
    if (!item) return def;
    return cJSON_GetNumberValue(item);
}

static bool safeBool(const cJSON *item, bool def) {
    if (!item) return def;
    if (item->type == cJSON_True) return true;
    if (item->type == cJSON_False) return false;
    return def;
}

static int parseDistribution(const cJSON *obj, DesDistribution *dist) {
    if (!obj) { dist->type = DES_DIST_FIXED; dist->param1 = 1; dist->param2 = 0; return 0; }
    const char *type = safeString(cJSON_GetObjectItem(obj, "distribution"), "fixed");
    if (strcmp(type, "fixed") == 0) dist->type = DES_DIST_FIXED;
    else if (strcmp(type, "uniform") == 0) dist->type = DES_DIST_UNIFORM;
    else if (strcmp(type, "exponential") == 0) dist->type = DES_DIST_EXPONENTIAL;
    else if (strcmp(type, "normal") == 0) dist->type = DES_DIST_NORMAL;
    else dist->type = DES_DIST_FIXED;
    dist->param1 = safeNumber(cJSON_GetObjectItem(obj, "param1"), 1.0);
    dist->param2 = safeNumber(cJSON_GetObjectItem(obj, "param2"), 0.0);
    return 0;
}

static int parseActionType(const char *action) {
    if (!action) return DES_ACTION_NONE;
    if (strcmp(action, "acquire_and_process") == 0) return DES_ACTION_ACQUIRE_AND_PROCESS;
    if (strcmp(action, "release_and_dispatch") == 0) return DES_ACTION_RELEASE_AND_DISPATCH;
    if (strcmp(action, "release_and_retry") == 0) return DES_ACTION_RELEASE_AND_RETRY;
    if (strcmp(action, "wait_retry") == 0) return DES_ACTION_WAIT_RETRY;
    if (strcmp(action, "entity_enter") == 0) return DES_ACTION_ENTITY_ENTER;
    if (strcmp(action, "entity_exit") == 0) return DES_ACTION_ENTITY_EXIT;
    if (strcmp(action, "none") == 0) return DES_ACTION_NONE;
    return DES_ACTION_NONE;
}

static int findResourceIndex(const DesSimConfig *cfg, const char *name) {
    for (int i = 0; i < cfg->num_resources; i++) {
        if (strcmp(cfg->resources[i].name, name) == 0) return i;
    }
    return DES_INVALID_ID;
}

static int findStageIndex(const DesSimConfig *cfg, const char *name) {
    for (int i = 0; i < cfg->num_stages; i++) {
        if (strcmp(cfg->stages[i].name, name) == 0) return i;
    }
    return DES_INVALID_ID;
}

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

DesSimConfig *DesConfig_loadJsonString(const char *json_string) {
    g_load_error[0] = '\0';

    cJSON *root = cJSON_Parse(json_string);
    if (!root) {
        set_load_error("invalid JSON syntax", NULL);
        return NULL;
    }

    DesSimConfig *cfg = DesConfig_create();

    cJSON *sim = cJSON_GetObjectItem(root, "simulation");
    if (sim) {
        cfg->max_time = (int)safeNumber(cJSON_GetObjectItem(sim, "max_time"), 100000);
        cfg->max_events = (int)safeNumber(cJSON_GetObjectItem(sim, "max_events"), DES_MAX_EVENTS);
        cfg->seed = (unsigned int)safeNumber(cJSON_GetObjectItem(sim, "seed"), 0);
        cfg->entity_capacity = (int)safeNumber(cJSON_GetObjectItem(sim, "entity_capacity"), 0);
    }

    cJSON *resources = cJSON_GetObjectItem(root, "resources");
    if (resources) {
        int size = cJSON_GetArraySize(resources);
        for (int i = 0; i < size; i++) {
            cJSON *res = cJSON_GetArrayItem(resources, i);
            const char *name = safeString(cJSON_GetObjectItem(res, "name"), "unknown");
            int count = (int)safeNumber(cJSON_GetObjectItem(res, "count"), 1);
            int res_id = DesConfig_addResource(cfg, name, count);
            cJSON *avail = cJSON_GetObjectItem(res, "available_at");
            if (avail && res_id != DES_INVALID_ID) {
                DesConfig_setResourceAvailableAt(cfg, res_id, (int)cJSON_GetNumberValue(avail));
            }
        }
    }

    cJSON *stages = cJSON_GetObjectItem(root, "stages");
    if (stages) {
        int size = cJSON_GetArraySize(stages);

        for (int i = 0; i < size; i++) {
            cJSON *stage = cJSON_GetArrayItem(stages, i);
            const char *name = safeString(cJSON_GetObjectItem(stage, "name"), "unknown");
            int stage_id = DesConfig_addStage(cfg, name);

            const char *res_name = safeString(cJSON_GetObjectItem(stage, "resource"), NULL);
            cJSON *mode_item = cJSON_GetObjectItem(stage, "mode");
            const char *mode = mode_item ? safeString(mode_item, "manual") : "manual";

            if (res_name) {
                int res_id = findResourceIndex(cfg, res_name);
                if (res_id == DES_INVALID_ID) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "stage '%s': unknown resource '%s'", name, res_name);
                    set_load_error(buf, NULL);
                    continue;
                }

                if (strcmp(mode, "resource") == 0) {
                    DesDistribution dist;
                    parseDistribution(cJSON_GetObjectItem(stage, "processing_time"), &dist);
                    DesStage_setResourceMode(cfg, stage_id, res_id, dist.type, dist.param1, dist.param2);
                } else {
                    DesStage_setResource(cfg, stage_id, res_id);
                }
            }

            if (strcmp(mode, "manual") == 0) {
                cJSON *states = cJSON_GetObjectItem(stage, "states");
                if (states) {
                    int n = cJSON_GetArraySize(states);
                    for (int j = 0; j < n; j++) {
                        cJSON *s = cJSON_GetArrayItem(states, j);
                        DesStage_addState(cfg, stage_id, cJSON_GetStringValue(s));
                    }
                }

                cJSON *event_types = cJSON_GetObjectItem(stage, "event_types");
                if (event_types) {
                    int n = cJSON_GetArraySize(event_types);
                    for (int j = 0; j < n; j++) {
                        cJSON *e = cJSON_GetArrayItem(event_types, j);
                        DesStage_addEventType(cfg, stage_id, cJSON_GetStringValue(e));
                    }
                }

                cJSON *proc_time = cJSON_GetObjectItem(stage, "processing_time");
                if (proc_time) {
                    DesDistribution dist;
                    parseDistribution(proc_time, &dist);
                    DesStage_setProcessingTime(cfg, stage_id, dist.type, dist.param1, dist.param2);
                }

                cJSON *fsm = cJSON_GetObjectItem(stage, "fsm");
                if (fsm) {
                    int n = cJSON_GetArraySize(fsm);
                    for (int j = 0; j < n; j++) {
                        cJSON *tr = cJSON_GetArrayItem(fsm, j);
                        const char *from = safeString(cJSON_GetObjectItem(tr, "state"), "IDLE");
                        const char *evt = safeString(cJSON_GetObjectItem(tr, "event"), "");
                        const char *to = safeString(cJSON_GetObjectItem(tr, "next_state"), "IDLE");
                        const char *action = safeString(cJSON_GetObjectItem(tr, "action"), "none");
                        DesStage_addTransition(cfg, stage_id, from, evt, to, parseActionType(action));
                    }
                }
            }
        }

        for (int i = 0; i < size; i++) {
            cJSON *stage = cJSON_GetArrayItem(stages, i);
            cJSON *outcomes = cJSON_GetObjectItem(stage, "outcomes");
            if (outcomes) {
                int n = cJSON_GetArraySize(outcomes);
                for (int j = 0; j < n; j++) {
                    cJSON *o = cJSON_GetArrayItem(outcomes, j);
                    double prob = safeNumber(cJSON_GetObjectItem(o, "probability"), 1.0);
                    const char *next_stage = safeString(cJSON_GetObjectItem(o, "next_stage"), NULL);
                    const char *next_event = safeString(cJSON_GetObjectItem(o, "next_event"), NULL);
                    const char *oname = safeString(cJSON_GetObjectItem(o, "name"), "outcome");
                    DesStage_addOutcome(cfg, i, prob, next_stage, next_event, oname);
                }
            }
        }
    }

    cJSON *arrivals = cJSON_GetObjectItem(root, "entity_arrivals");
    if (arrivals) {
        int size = cJSON_GetArraySize(arrivals);
        for (int i = 0; i < size; i++) {
            cJSON *a = cJSON_GetArrayItem(arrivals, i);
            const char *name = safeString(cJSON_GetObjectItem(a, "name"), "Entity");
            int count = (int)safeNumber(cJSON_GetObjectItem(a, "count"), 100);
            const char *entry = safeString(cJSON_GetObjectItem(a, "entry_stage"), NULL);
            DesDistribution dist;
            parseDistribution(cJSON_GetObjectItem(a, "inter_arrival"), &dist);
            int arr_id = DesConfig_addArrival(cfg, name, count, entry, dist.type, dist.param1, dist.param2);
            if (arr_id != DES_INVALID_ID) {
                cJSON *st = cJSON_GetObjectItem(a, "start_time");
                if (st) DesConfig_setArrivalStart(cfg, arr_id, (int)cJSON_GetNumberValue(st));
                cJSON *pr = cJSON_GetObjectItem(a, "priority");
                if (pr) DesConfig_setArrivalPriority(cfg, arr_id, (int)cJSON_GetNumberValue(pr));
            }
        }
    }

    cJSON *stats = cJSON_GetObjectItem(root, "statistics");
    if (stats) {
        bool re  = safeBool(cJSON_GetObjectItem(stats, "record_events"), true);
        bool ref = safeBool(cJSON_GetObjectItem(stats, "record_entity_flow"), true);
        bool ru  = safeBool(cJSON_GetObjectItem(stats, "record_resource_util"), false);
        const char *dir = safeString(cJSON_GetObjectItem(stats, "output_dir"), "./output");
        DesConfig_setStats(cfg, re, ref, ru, dir);
    }

    cJSON_Delete(root);
    return cfg;
}

DesSimConfig *DesConfig_loadJson(const char *filepath) {
    g_load_error[0] = '\0';
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        set_load_error("cannot open file", filepath);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        set_load_error("empty file", filepath);
        return NULL;
    }
    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(f); set_load_error("out of memory", NULL); return NULL; }
    size_t read = fread(buf, 1, (size_t)size, f);
    buf[read] = '\0';
    fclose(f);
    DesSimConfig *cfg = DesConfig_loadJsonString(buf);
    free(buf);
    if (!cfg && g_load_error[0] == '\0') {
        set_load_error("failed to parse JSON", filepath);
    }
    return cfg;
}
