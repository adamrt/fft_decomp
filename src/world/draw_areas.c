/* The palette mode and the temporary and next ordering table draw areas. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "fft/world_gfx.h"
#include "fft/world_menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

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
        g_menu_palette_clut = g_world_menu_palette_clut_alt;
        g_world_menu_disabled_number_clut = g_world_menu_disabled_number_clut_alt;
        g_world_menu_active_clut_front = g_menu_cursor_mode1_foreground_clut;
        clut = g_menu_cursor_mode1_background_clut;
    } else {
        g_world_menu_sprite_color.r = 0x80;
        g_world_menu_sprite_color.g = 0x80;
        g_world_menu_sprite_color.b = 0x80;
        g_world_menu_window_clut = g_world_menu_window_clut_normal;
        g_world_menu_alternate_number_clut = g_world_menu_alternate_number_clut_normal;
        g_menu_palette_clut = g_world_menu_palette_clut_normal;
        g_world_menu_disabled_number_clut = g_world_menu_disabled_number_clut_normal;
        g_world_menu_active_clut_front = g_menu_cursor_mode0_foreground_clut;
        clut = g_menu_cursor_mode0_background_clut;
    }
    g_world_menu_active_clut_back = clut;
}

/* Remove the active temporary draw-area clip and advance the menu script. */
menu_instruction_t* world_menu_script_clear_temporary_draw_area(menu_instruction_t* instruction) {
    if (g_gfx_otag_index_locked != 0) {
        g_world_gfx_temporary_draw_area.y = 0;
        world_gfx_add_draw_area_primitive(&g_world_gfx_temporary_draw_area, g_menu_gfx_otag_index + 1);
        g_gfx_otag_index_locked = 0;
    }
    return menu_next_instruction(instruction);
}

/* Apply a four-byte draw area from the menu script and advance the command. */
menu_instruction_t* world_menu_script_set_draw_area(menu_instruction_t* instruction) {
    RECT rect;

    if (g_gfx_otag_index_locked == 0) {
        rect.x = instruction->draw_area.x;
        rect.y = instruction->draw_area.y;
        rect.w = instruction->draw_area.width;
        rect.h = instruction->draw_area.height;
        world_gfx_add_draw_area_primitive(&rect, g_menu_gfx_otag_index - 1);
    }
    return menu_next_instruction(instruction);
}

/* Menu-script opcode handler: unless the flag is set, clears the y of
 * g_world_gfx_temporary_draw_area and re-registers the draw area at draw
 * priority + 1. Returns the address of the next instruction. */
menu_instruction_t* menu_cmd_add_next_otag_draw_area_handler_variant(menu_instruction_t* instruction) {
    if (g_gfx_otag_index_locked == 0) {
        g_world_gfx_temporary_draw_area.y = 0;
        world_gfx_add_draw_area_primitive(&g_world_gfx_temporary_draw_area, g_menu_gfx_otag_index + 1);
    }
    return menu_next_instruction(instruction);
}
