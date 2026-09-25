#include "fft/battle.h"

void battle_effect_restore_inertia_threshold(void) {
    g_battle_effect_inertia_threshold = g_battle_effect_saved_inertia_threshold;
}
