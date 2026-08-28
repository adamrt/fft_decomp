#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "psx/types.h"

s32 battle_effect_calculate_launch_velocity(void) {
    return SquareRoot12((g_battle_effect_gravity_modifier * g_battle_effect_gravity_scale) >> 12);
}
