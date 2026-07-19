#ifndef DES_ENTITY_H
#define DES_ENTITY_H

#include "des_types.h"

struct DesEngine;

int            DesEntity_create(struct DesEngine *engine, int entry_stage_id);
void           DesEntity_enterStage(struct DesEngine *engine, int entity_id, int stage_id);
void           DesEntity_exitSystem(struct DesEngine *engine, int entity_id, int outcome_id);
const DesEntity *DesEntity_get(const struct DesEngine *engine, int entity_id);

#endif
