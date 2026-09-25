#include "fft/battle.h"
#include "psx/types.h"

void battle_gfx_draw_screen_color_modulation_overlay(void) {
    char unused[24];

    if ((g_battle_gfx_screen_color_modulation[0] | g_battle_gfx_screen_color_modulation[1]
            | g_battle_gfx_screen_color_modulation[2])
        != 0) {
        g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity].r0
            = g_battle_gfx_screen_color_modulation[0];
        g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity].g0
            = g_battle_gfx_screen_color_modulation[1];
        g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity].b0
            = g_battle_gfx_screen_color_modulation[2];
        AddPrim(main_gfx_get_otag(), &g_battle_gfx_screen_modulation_polygons[g_main_gfx_screen_polarity]);
        AddPrim(main_gfx_get_otag(), (u8*)g_battle_gfx_screen_modulation_draw_modes + g_main_gfx_screen_polarity * 0xc);
    }
}
