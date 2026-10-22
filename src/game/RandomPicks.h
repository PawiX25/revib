#ifndef RANDOMPICKS_H
#define RANDOMPICKS_H

#include "common.h"

/*
 * `count` random numbers from `lo` to `hi` into `picks`, from `seed`:
 * all different until every number has been picked, then again.
 */
void RandomPicks(int lo, int hi, int count, u_long seed, int* picks);

#endif
