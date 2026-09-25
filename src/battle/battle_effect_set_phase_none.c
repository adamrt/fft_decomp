#include "fft/battle.h"

void battle_effect_set_phase_none(void) {
    if (g_battle_effect_phase == 1) {
        g_battle_effect_phase = 0;
    }
}
