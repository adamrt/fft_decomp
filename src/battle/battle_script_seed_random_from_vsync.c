#include "fft/battle.h"
#include "psx/types.h"

void battle_script_seed_random_from_vsync(void) {
    g_battle_script_rand16_state = VSync(-1);
}
