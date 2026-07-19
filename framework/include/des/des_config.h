#ifndef DES_CONFIG_H
#define DES_CONFIG_H

#include "des_types.h"

DesSimConfig *DesConfig_create(void);
void          DesConfig_initValue(DesSimConfig *cfg);
void          DesConfig_destroy(DesSimConfig *cfg);

const char   *DesConfig_getLastError(const DesSimConfig *cfg);
bool          DesConfig_validate(const DesSimConfig *cfg, DesValidationResult *result);
DesErrorCode  DesConfig_saveJson(const DesSimConfig *cfg, const char *filepath);

/* --- Builder API --- */

int  DesConfig_addResource(DesSimConfig *cfg, const char *name, int count);
void DesConfig_setResourceAvailableAt(DesSimConfig *cfg, int resource_id, int time);
int  DesConfig_addStage(DesSimConfig *cfg, const char *name);
void DesStage_setResource(DesSimConfig *cfg, int stage_id, int resource_type_id);
void DesStage_setProcessingTime(DesSimConfig *cfg, int stage_id,
                                DesDistType dist, double p1, double p2);
void DesStage_setInitialState(DesSimConfig *cfg, int stage_id, int state_id);

int  DesStage_addState(DesSimConfig *cfg, int stage_id, const char *state_name);
int  DesStage_addEventType(DesSimConfig *cfg, int stage_id, const char *event_name);

/*  Shorthand: auto-generates IDLE/BUSY states, ENTER/COMPLETE events,
    and the standard acquire_and_process / release_and_dispatch FSM. */
void DesStage_setResourceMode(DesSimConfig *cfg, int stage_id, int resource_type_id,
                              DesDistType dist, double p1, double p2);

void DesStage_addTransition(DesSimConfig *cfg, int stage_id,
                            const char *from_state, const char *event_name,
                            const char *to_state, DesActionType action);
void DesStage_addTransitionIdx(DesSimConfig *cfg, int stage_id,
                               int from_state, int event, int to_state,
                               DesActionType action);
void DesStage_addOutcome(DesSimConfig *cfg, int stage_id,
                         double probability, const char *next_stage_name,
                         const char *next_event_name, const char *outcome_name);
void DesStage_addOutcomeIdx(DesSimConfig *cfg, int stage_id,
                            double probability, int next_stage_id,
                            int next_event_index, const char *outcome_name);
int  DesConfig_addArrival(DesSimConfig *cfg, const char *entity_name,
                          int count, const char *entry_stage,
                          DesDistType dist, double p1, double p2);
int  DesConfig_addArrivalIdx(DesSimConfig *cfg, const char *entity_name,
                             int count, int entry_stage_id,
                             DesDistType dist, double p1, double p2);

void DesConfig_setArrivalStart(DesSimConfig *cfg, int arrival_id, int start_time);
void DesConfig_setArrivalPriority(DesSimConfig *cfg, int arrival_id, int priority);
void DesConfig_setMaxTime(DesSimConfig *cfg, int max_time);
void DesConfig_setMaxEvents(DesSimConfig *cfg, int max_events);
void DesConfig_setEntityCapacity(DesSimConfig *cfg, int capacity);
void DesConfig_setSeed(DesSimConfig *cfg, unsigned int seed);
void DesConfig_setName(DesSimConfig *cfg, const char *name);
void DesConfig_setStats(DesSimConfig *cfg, bool record_events,
                        bool record_entity_flow, bool record_resource_util,
                        const char *output_dir);

/* --- Setters for editing --- */
void DesConfig_setResourceName(DesSimConfig *cfg, int resource_id, const char *name);
void DesConfig_setResourceCount(DesSimConfig *cfg, int resource_id, int count);
void DesStage_setName(DesSimConfig *cfg, int stage_id, const char *name);
void DesConfig_setArrivalName(DesSimConfig *cfg, int arrival_id, const char *name);
void DesConfig_setArrivalCount(DesSimConfig *cfg, int arrival_id, int count);

/* --- Remove operations --- */
int  DesConfig_removeResource(DesSimConfig *cfg, int resource_id);
int  DesConfig_removeStage(DesSimConfig *cfg, int stage_id);
int  DesConfig_removeArrival(DesSimConfig *cfg, int arrival_id);

/* --- Convenience macros --- */

#define DesConfig_init()  DesConfig_create()

#define DES_DIST_FIX(v, d)      do { (d).type = DES_DIST_FIXED;      (d).param1 = (v); (d).param2 = 0; } while(0)
#define DES_DIST_UNI(a, b, d)   do { (d).type = DES_DIST_UNIFORM;    (d).param1 = (a); (d).param2 = (b); } while(0)
#define DES_DIST_EXP(lam, d)    do { (d).type = DES_DIST_EXPONENTIAL; (d).param1 = (lam); (d).param2 = 0; } while(0)
#define DES_DIST_NORM(m, s, d)  do { (d).type = DES_DIST_NORMAL;     (d).param1 = (m); (d).param2 = (s); } while(0)

/* Backward compat */
#define DES_DIST_FMT  DES_DIST_FIX
#define DES_DIST_NRM  DES_DIST_NORM

#endif
