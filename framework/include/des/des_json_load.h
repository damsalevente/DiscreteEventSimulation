#ifndef DES_JSON_LOAD_H
#define DES_JSON_LOAD_H

#include "des_types.h"

DesSimConfig *DesConfig_loadJson(const char *filepath);
DesSimConfig *DesConfig_loadJsonString(const char *json_string);
const char   *DesConfig_getLoadError(void);

#endif
