#include "fft/battle_gfx.h"
#include "psx/types.h"

void battle_gfx_copy_screen_color_modulation(void) {
    g_battle_gfx_screen_color_modulation_backup[0] = g_battle_gfx_screen_color_modulation[0];
    g_battle_gfx_screen_color_modulation_backup[1] = g_battle_gfx_screen_color_modulation[1];
    g_battle_gfx_screen_color_modulation_backup[2] = g_battle_gfx_screen_color_modulation[2];
}
