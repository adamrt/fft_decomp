#ifndef RANDOM_H
#define RANDOM_H

/* Random ranges for effect motion and threshold rolls for game rules. */

#include "psx/libc.h"
#include "psx/types.h"

/* Includes the lower bound and excludes the upper, in either argument order;
 * equal bounds return that value without drawing a random number. Spelled
 * with rand() first, unlike the effect ribbon's local RAND_BETWEEN. */
#define RANDOM_BETWEEN(min, max)                                                                                       \
    ((min) == (max) ? (min) : (max) < (min) ? rand() % ((min) - (max)) + (max) : rand() % ((max) - (min)) + (min))

/* Zero means the roll is below threshold; one means it failed. */
s32 random_roll_pass_fail(s32 maximum, s32 threshold);

#endif
