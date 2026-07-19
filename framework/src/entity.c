#include "des/des_entity.h"
#include "des/des_engine.h"
#include <string.h>

int DesEntity_create(DesEngine *engine, int entry_stage_id) {
    if (engine->next_entity_id >= engine->entity_capacity) return DES_INVALID_ID;
    int id = engine->next_entity_id++;
    DesEntity *e = &engine->entities[id];
    memset(e, 0, sizeof(DesEntity));
    e->id = id;
    e->current_stage_id = entry_stage_id;
    e->current_state = engine->stages[entry_stage_id].initial_state;
    e->entry_time = engine->current_time;
    e->stage_entry_time = engine->current_time;
    e->completion_time = -1;
    e->outcome_id = -1;
    e->active = true;
    e->num_stage_visits = 0;
    e->user_data = NULL;
    engine->num_active_entities++;
    return id;
}

void DesEntity_enterStage(DesEngine *engine, int entity_id, int stage_id) {
    if (entity_id < 0 || entity_id >= engine->next_entity_id) return;
    DesEntity *e = &engine->entities[entity_id];
    e->current_stage_id = stage_id;
    e->current_state = engine->stages[stage_id].initial_state;
    e->stage_entry_time = engine->current_time;
    e->num_stage_visits++;
}

void DesEntity_exitSystem(DesEngine *engine, int entity_id, int outcome_id) {
    if (entity_id < 0 || entity_id >= engine->next_entity_id) return;
    DesEntity *e = &engine->entities[entity_id];
    e->completion_time = engine->current_time;
    e->outcome_id = outcome_id;
    e->active = false;
    e->current_stage_id = DES_INVALID_ID;
    engine->num_active_entities--;
    engine->num_completed_entities++;
}

const DesEntity *DesEntity_get(const DesEngine *engine, int entity_id) {
    if (entity_id < 0 || entity_id >= engine->next_entity_id) return NULL;
    return &engine->entities[entity_id];
}
