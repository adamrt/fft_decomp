#include "fft/equip.h"
#include "psx/types.h"

/* Select the compact or full equip window layout. */
void equip_text_set_palette_and_metrics(s32 compact) {
    g_equip_text_compact_layout = compact;
    if (compact != 0) {
        g_equip_gfx_sprite_color[0] = 0x60;
        g_equip_gfx_sprite_color[1] = 0x60;
        g_equip_gfx_sprite_color[2] = 0x80;
        g_equip_text_metric_0 = g_equip_text_clut_0_mode1;
        g_equip_text_metric_1 = g_equip_text_clut_1_mode1;
        g_equip_text_metric_2 = g_equip_text_clut_2_mode1;
        g_equip_text_metric_3 = g_equip_text_clut_3_mode1;
        g_equip_text_metric_4 = g_equip_menu_cursor_mode1_foreground_clut;
        g_equip_text_metric_5 = g_equip_menu_cursor_mode1_background_clut;
    } else {
        g_equip_gfx_sprite_color[0] = 0x80;
        g_equip_gfx_sprite_color[1] = 0x80;
        g_equip_gfx_sprite_color[2] = 0x80;
        g_equip_text_metric_0 = g_equip_text_clut_0_mode0;
        g_equip_text_metric_1 = g_equip_text_clut_1_mode0;
        g_equip_text_metric_2 = g_equip_text_clut_2_mode0;
        g_equip_text_metric_3 = g_equip_text_clut_3_mode0;
        g_equip_text_metric_4 = g_equip_menu_cursor_mode0_foreground_clut;
        g_equip_text_metric_5 = g_equip_menu_cursor_mode0_background_clut;
    }
}
