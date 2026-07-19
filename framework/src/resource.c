#include "des/des_resource.h"
#include "des/des_engine.h"
#include <string.h>

int DesResource_acquire(DesEngine *engine, int resource_type_id) {
    if (!engine) return DES_INVALID_ID;
    if (resource_type_id < 0 || resource_type_id >= engine->num_resource_types) return -1;
    DesResourceType *rt = &engine->resource_types[resource_type_id];

    int start = rt->first_instance_idx;
    int end = start + rt->instance_count;
    for (int i = start; i < end; i++) {
        DesResourceInstance *ri = &engine->resource_instances[i];
        if (ri->assigned_entity == DES_INVALID_ID) {
            if (ri->available_at_time > engine->current_time) continue;
            ri->assigned_entity = 0;
            rt->available--;
            return ri->instance_id;
        }
    }
    return -1;
}

void DesResource_release(DesEngine *engine, int resource_type_id, int instance_id) {
    if (!engine) return;
    if (resource_type_id < 0 || resource_type_id >= engine->num_resource_types) return;
    DesResourceType *rt = &engine->resource_types[resource_type_id];

    int start = rt->first_instance_idx;
    int end = start + rt->instance_count;
    for (int i = start; i < end; i++) {
        DesResourceInstance *ri = &engine->resource_instances[i];
        if (ri->instance_id == instance_id) {
            if (ri->assigned_entity == DES_INVALID_ID) return;
            ri->assigned_entity = DES_INVALID_ID;
            ri->assigned_stage = DES_INVALID_ID;
            rt->available++;
            return;
        }
    }
}

int DesResource_getAvailable(const DesEngine *engine, int resource_type_id) {
    if (!engine) return 0;
    if (resource_type_id < 0 || resource_type_id >= engine->num_resource_types) return 0;
    const DesResourceType *rt = &engine->resource_types[resource_type_id];
    int available = 0;
    int end = rt->first_instance_idx + rt->instance_count;
    for (int i = rt->first_instance_idx; i < end; i++) {
        const DesResourceInstance *instance = &engine->resource_instances[i];
        if (instance->assigned_entity == DES_INVALID_ID &&
            instance->available_at_time <= engine->current_time)
            available++;
    }
    return available;
}
