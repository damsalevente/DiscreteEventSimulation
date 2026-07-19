#ifndef DES_RNG_H
#define DES_RNG_H

#include "des_types.h"

struct DesEngine;

void DesRng_setSeed(struct DesEngine *engine, unsigned int seed);
int  DesRng_exponential(struct DesEngine *engine, double lambda);
int  DesRng_uniform(struct DesEngine *engine, int min, int max);
int  DesRng_normalInt(struct DesEngine *engine, double mean, double stddev);
int  DesRng_sample(struct DesEngine *engine, const DesDistribution *dist);
bool DesRng_coinFlip(struct DesEngine *engine, double probability);
int  DesRng_selectOutcome(struct DesEngine *engine, const DesStageOutcome *outcomes,
                          int num_outcomes);

#endif
