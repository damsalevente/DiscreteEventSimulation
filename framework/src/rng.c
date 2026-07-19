#define _USE_MATH_DEFINES
#include "des/des_rng.h"
#include "des/des_engine.h"
#include <math.h>

void DesRng_setSeed(DesEngine *engine, unsigned int seed) {
    engine->rng_state = seed;
}

static unsigned int rngNext(DesEngine *engine) {
    engine->rng_state = engine->rng_state * 1103515245 + 12345;
    return (engine->rng_state >> 16) & 0x7FFF;
}

int DesRng_exponential(DesEngine *engine, double lambda) {
    double u = (double)rngNext(engine) / 32768.0;
    if (u >= 1.0) u = 0.999999;
    if (u <= 0.0) u = 0.000001;
    return (int)floor(-log(1.0 - u) / lambda);
}

int DesRng_uniform(DesEngine *engine, int min, int max) {
    if (min >= max) return min;
    return min + (int)(rngNext(engine) % (unsigned)(max - min + 1));
}

int DesRng_normalInt(DesEngine *engine, double mean, double stddev) {
    double u1 = (double)rngNext(engine) / 32768.0;
    double u2 = (double)rngNext(engine) / 32768.0;
    if (u1 < 0.0001) u1 = 0.0001;
    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    return (int)floor(mean + stddev * z);
}

int DesRng_sample(DesEngine *engine, const DesDistribution *dist) {
    switch (dist->type) {
        case DES_DIST_FIXED:
            return (int)dist->param1;
        case DES_DIST_UNIFORM:
            return DesRng_uniform(engine, (int)dist->param1, (int)dist->param2);
        case DES_DIST_EXPONENTIAL:
            return DesRng_exponential(engine, dist->param1);
        case DES_DIST_NORMAL:
            return DesRng_normalInt(engine, dist->param1, dist->param2);
    }
    return 0;
}

bool DesRng_coinFlip(DesEngine *engine, double probability) {
    double u = (double)rngNext(engine) / 32768.0;
    return u < probability;
}

int DesRng_selectOutcome(DesEngine *engine, const DesStageOutcome *outcomes,
                         int num_outcomes) {
    double roll = (double)rngNext(engine) / 32768.0;
    double cumulative = 0.0;
    for (int i = 0; i < num_outcomes; i++) {
        cumulative += outcomes[i].probability;
        if (roll <= cumulative) return i;
    }
    return num_outcomes - 1;
}
