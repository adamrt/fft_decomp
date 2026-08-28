#include "fft/battle_effect.h"
#include "psx/types.h"

void battle_effect_reset_parameter_sets_pointer(void) {
    g_battle_effect_parameter_sets = g_battle_effect_parameter_sets_start;
}
