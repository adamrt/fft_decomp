#include "fft/battle.h"
#include "psx/types.h"

/* Return 1 when a random value in [0, limit) is at least threshold, otherwise 0. */
s32 battle_formula_roll_random_at_least(s32 limit, s32 threshold) {
    return (s32)(battle_formula_get_random_0_7fff() * limit) / 32768 >= threshold;
}
