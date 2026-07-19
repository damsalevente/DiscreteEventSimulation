#ifndef DES_STATS_H
#define DES_STATS_H

#include "des_types.h"

struct DesEngine;

void DesStats_init(DesStatsCollector *stats);
void DesStats_destroy(DesStatsCollector *stats);
void DesStats_recordEvent(struct DesEngine *engine, const DesEvent *event);
void DesStats_recordEntityTransition(struct DesEngine *engine, int entity_id,
                                     int stage_id, int enter_time, int exit_time,
                                     int outcome_id);
void DesStats_recordResourceState(struct DesEngine *engine, int time,
                                  int resource_type_id, int instance_id,
                                  int state, int entity_id);
void DesStats_generateReport(const struct DesEngine *engine);
void DesStats_printSummary(const struct DesEngine *engine);
void DesStats_printConfigSummary(const struct DesEngine *engine);

#endif
