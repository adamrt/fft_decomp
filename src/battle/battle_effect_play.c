#include "fft/battle.h"

void battle_effect_play(void) {
    if (g_battle_effect_phase == 1) {
        g_battle_effect_phase = 2;
    }
}
