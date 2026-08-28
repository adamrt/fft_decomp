#include "fft/world.h"

/*
 * Select the normal or alternate menu-render palette.
 *
 * The mode updates the shared text color and six related CLUT selections as a
 * single visual theme.
 */
void world_menu_set_palette_mode(s32 mode) {
    u16 clut;

    g_world_menu_use_alternate_palette = mode;
    if (mode != 0) {
        g_world_menu_sprite_color.r = 0x60;
        g_world_menu_sprite_color.g = 0x60;
        g_world_menu_sprite_color.b = 0x80;
        g_world_menu_window_clut = g_world_menu_window_clut_alt;
        g_world_menu_alternate_number_clut = g_world_menu_alternate_number_clut_alt;
        g_world_menu_palette_clut = g_world_menu_palette_clut_alt;
        g_world_menu_disabled_number_clut = g_world_menu_disabled_number_clut_alt;
        g_world_menu_active_clut_front = g_world_menu_clut_front_alt;
        clut = g_world_menu_clut_back_alt;
    } else {
        g_world_menu_sprite_color.r = 0x80;
        g_world_menu_sprite_color.g = 0x80;
        g_world_menu_sprite_color.b = 0x80;
        g_world_menu_window_clut = g_world_menu_window_clut_normal;
        g_world_menu_alternate_number_clut = g_world_menu_alternate_number_clut_normal;
        g_world_menu_palette_clut = g_world_menu_palette_clut_normal;
        g_world_menu_disabled_number_clut = g_world_menu_disabled_number_clut_normal;
        g_world_menu_active_clut_front = g_world_menu_clut_front;
        clut = g_world_menu_clut_back;
    }
    g_world_menu_active_clut_back = clut;
}
