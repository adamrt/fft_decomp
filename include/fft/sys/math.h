#ifndef MATH_H
#define MATH_H

/* Math helpers: the random number generator and small vector and fixed-point helpers. */

#include "psx/libc.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* random numbers */
/* Includes the lower bound and excludes the upper, in either argument order;
 * equal bounds return that value without drawing a random number. Spelled
 * with rand() first, unlike the effect ribbon's local RAND_BETWEEN. */
#define RANDOM_BETWEEN(min, max)                                                                                       \
    ((min) == (max) ? (min) : (max) < (min) ? rand() % ((min) - (max)) + (max) : rand() % ((max) - (min)) + (min))

/* Zero means the roll is below threshold; one means it failed. */
s32 random_roll_pass_fail(s32 maximum, s32 threshold);

/* vectors */
/* Resident coordinate setters for the SDK's short and full-width vectors. */
void vector_set_short(SVECTOR* vector, s32 x, s32 y, s32 z);
void vector_set_long(VECTOR* vector, s32 x, s32 y, s32 z);

/* fixed-point math */
/* No C source: hand-written MIPS in the original, kept as original bytes through the `regions:` rows of
 * target/battle.yaml and target/world.yaml (a 64-round signed divide and a fixed-point multiply with overflow check); a
 * port supplies both bodies. QUIRKS.md records their rounding and carry bugs. */
s32 math_mul_div_s64(s32 a, s32 b, s32 c);
s32 math_cross_product_q12(s32 a, s32 b, s32 c, s32 d);

#endif
