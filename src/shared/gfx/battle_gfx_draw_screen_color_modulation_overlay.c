#include "fft/battle_gfx.h"

/* Queues the screen color-modulation overlay for this frame, the polygon and its
 * draw mode of the current screen polarity, unless all three components are 0.
 * `unused` reproduces the frame size. */
void battle_gfx_draw_screen_color_modulation_overlay(void) {
    char unused[24];

    if ((g_battle_gfx_screen_color_modulation.r | g_battle_gfx_screen_color_modulation.g
            | g_battle_gfx_screen_color_modulation.b)
        != 0) {
        g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity].r0 = g_battle_gfx_screen_color_modulation.r;
        g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity].g0 = g_battle_gfx_screen_color_modulation.g;
        g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity].b0 = g_battle_gfx_screen_color_modulation.b;
        AddPrim(main_gfx_get_otag(), &g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity]);
        AddPrim(main_gfx_get_otag(), &g_battle_gfx_screen_modulation_draw_modes[g_main_gfx_screen_polarity]);
    }
}
