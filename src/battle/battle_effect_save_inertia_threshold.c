#include "fft/battle_effect.h"

void battle_effect_save_inertia_threshold(void) {
    g_battle_effect_saved_inertia_threshold = g_battle_effect_inertia_threshold;
}
