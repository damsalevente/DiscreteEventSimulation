#include "des/des.h"
#include "des/des_mdf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    const char *config_path = "configs/coffee_shop.json";
    const char *out_path = "output/resources.mf4";
    if (argc > 1) config_path = argv[1];
    if (argc > 2) out_path = argv[2];

    printf("Loading config: %s\n", config_path);
    DesSimConfig *cfg = DesConfig_loadJson(config_path);
    if (!cfg) {
        fprintf(stderr, "Failed to load config: %s\n", config_path);
        return 1;
    }

    /* Ensure resource utilisation is recorded for the export. */
    cfg->stats.record_resource_util = true;

    printf("Resources: %d\n", cfg->num_resources);
    int total_inst = 0;
    for (int i = 0; i < cfg->num_resources; i++) {
        printf("  %s: %d instances\n", cfg->resources[i].name, cfg->resources[i].count);
        total_inst += cfg->resources[i].count;
    }
    printf("Total resource instances (one MDF channel each): %d\n", total_inst);

    DesEngine *engine = DesEngine_create(cfg);
    if (!engine) {
        fprintf(stderr, "Cannot create engine: invalid configuration or out of memory\n");
        DesConfig_destroy(cfg);
        return 2;
    }
    printf("Running simulation...\n");
    DesErrorCode ec = DesEngine_run(engine);
    if (ec != DES_OK) {
        fprintf(stderr, "Simulation error: %d\n", ec);
        DesEngine_destroy(engine);
        DesConfig_destroy(cfg);
        return 3;
    }

    printf("Writing MDF4 export: %s\n", out_path);
    DesErrorCode mec = DesMdf_exportResources(engine, out_path);
    if (mec != DES_OK) {
        fprintf(stderr, "MDF export failed: %d\n", mec);
        DesEngine_destroy(engine);
        DesConfig_destroy(cfg);
        return 4;
    }

    DesStats_generateReport(engine);

    DesEngine_destroy(engine);
    DesConfig_destroy(cfg);
    return 0;
}
