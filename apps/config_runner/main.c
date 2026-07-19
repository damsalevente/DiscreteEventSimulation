#include "des/des.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    const char *config_path = "configs/theme_park.json";
    if (argc > 1) config_path = argv[1];

    printf("Loading config: %s\n", config_path);
    DesSimConfig *cfg = DesConfig_loadJson(config_path);
    if (!cfg) {
        fprintf(stderr, "Failed to load config: %s\n", DesConfig_getLoadError());
        return 1;
    }

    DesEngine *engine = DesEngine_create(cfg);

    DesStats_printConfigSummary(engine);

    printf("Running simulation...\n");
    DesErrorCode ec = DesEngine_run(engine);
    if (ec != DES_OK) {
        fprintf(stderr, "\nSimulation error: %s\n", DesError_toString(ec));
    }

    DesStats_printSummary(engine);
    DesStats_generateReport(engine);

    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);

    return 0;
}
