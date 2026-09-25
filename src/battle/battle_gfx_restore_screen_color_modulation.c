#include "fft/battle.h"
#include "psx/types.h"

void battle_gfx_restore_screen_color_modulation(void) {
    g_battle_gfx_screen_color_modulation[0] = g_battle_gfx_screen_color_modulation_backup[0];
    g_battle_gfx_screen_color_modulation[1] = g_battle_gfx_screen_color_modulation_backup[1];
    g_battle_gfx_screen_color_modulation[2] = g_battle_gfx_screen_color_modulation_backup[2];
}
