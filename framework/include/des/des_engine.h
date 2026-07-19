#ifndef DES_ENGINE_H
#define DES_ENGINE_H

#include "des_types.h"

struct DesEngine {
    const DesSimConfig *config;
    int  current_time;
    int  next_event_id;
    int  events_processed;
    DesEventQueue queue;
    int       num_stages;
    DesStage *stages;
    int                num_resource_types;
    DesResourceType   *resource_types;
    DesResourceInstance *resource_instances;
    int                total_resource_instances;
    DesEntity *entities;
    int        entity_capacity;
    int        next_entity_id;
    int        num_active_entities;
    int        num_completed_entities;
    DesStatsCollector stats;
    DesCustomAction *custom_actions;
    int              num_custom_actions;
    unsigned int rng_state;
    bool running;
    bool seeded;
    bool error;
    DesErrorCode last_error;
};

DesEngine    *DesEngine_create(const DesSimConfig *config);
DesErrorCode  DesEngine_run(DesEngine *engine);
DesErrorCode  DesEngine_step(DesEngine *engine);
void          DesEngine_destroy(DesEngine *engine);
int           DesEngine_registerAction(DesEngine *engine, const char *name,
                                       DesActionFn fn, void *user_data);
int           DesEngine_getTime(const DesEngine *engine);
int           DesEngine_getEntityCount(const DesEngine *engine);
int           DesEngine_getResourceAvailable(const DesEngine *engine,
                                             int resource_type_id);
const char   *DesEngine_getStageName(const DesEngine *engine, int stage_id);

#endif
