#include "fft/battle.h"
#include "psx/types.h"

void battle_gfx_clear_screen_color_modulation(void) {
    g_battle_gfx_screen_color_modulation[2] = 0;
    g_battle_gfx_screen_color_modulation[1] = 0;
    g_battle_gfx_screen_color_modulation[0] = 0;
}
