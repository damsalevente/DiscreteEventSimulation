#ifndef DES_RESOURCE_H
#define DES_RESOURCE_H

#include "des_types.h"

struct DesEngine;

int  DesResource_acquire(struct DesEngine *engine, int resource_type_id);
void DesResource_release(struct DesEngine *engine, int resource_type_id, int instance_id);
int  DesResource_getAvailable(const struct DesEngine *engine, int resource_type_id);

#endif
