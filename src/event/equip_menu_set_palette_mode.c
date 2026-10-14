#include "fft/event_equip.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Select the normal (0) or alternate menu palette: the sprite color and the six CLUT selections
 * the menu commands draw with. Twin of world_menu_set_palette_mode. */
void equip_menu_set_palette_mode(s32 mode) {
    g_menu_use_alternate_palette = mode;
    if (mode != 0) {
        g_menu_gfx_sprite_color.r = 0x60;
        g_menu_gfx_sprite_color.g = 0x60;
        g_menu_gfx_sprite_color.b = 0x80;
        g_equip_menu_window_clut = g_equip_text_clut_0_mode1;
        g_equip_menu_alternate_number_clut = g_equip_text_clut_1_mode1;
        g_menu_palette_clut = g_equip_text_clut_2_mode1;
        g_equip_menu_disabled_number_clut = g_equip_text_clut_3_mode1;
        g_equip_menu_active_clut_front = g_menu_cursor_mode1_foreground_clut;
        g_equip_menu_active_clut_back = g_menu_cursor_mode1_background_clut;
    } else {
        g_menu_gfx_sprite_color.r = 0x80;
        g_menu_gfx_sprite_color.g = 0x80;
        g_menu_gfx_sprite_color.b = 0x80;
        g_equip_menu_window_clut = g_equip_text_clut_0_mode0;
        g_equip_menu_alternate_number_clut = g_equip_text_clut_1_mode0;
        g_menu_palette_clut = g_equip_text_clut_2_mode0;
        g_equip_menu_disabled_number_clut = g_equip_text_clut_3_mode0;
        g_equip_menu_active_clut_front = g_menu_cursor_mode0_foreground_clut;
        g_equip_menu_active_clut_back = g_menu_cursor_mode0_background_clut;
    }
}
