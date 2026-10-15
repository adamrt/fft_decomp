#include "fft/battle_gfx.h"

/* No retail code calls it. */
void battle_gfx_clear_screen_color_modulation(void) {
    g_battle_gfx_screen_color_modulation.b = 0;
    g_battle_gfx_screen_color_modulation.g = 0;
    g_battle_gfx_screen_color_modulation.r = 0;
}
