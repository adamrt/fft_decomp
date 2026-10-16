/* The palette mode and the temporary and next ordering table draw areas. */

#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "fft/world_gfx.h"
#include "fft/world_menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Select the normal or alternate menu-render palette.
 *
 * The mode updates the sprite modulation color and six related CLUT
 * selections as a single visual theme. The alternate look (sprites tinted to
 * 0x60/0x60/0x80) is what the shop and formation screens show while input is
 * locked to another window.
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

/* Menu-script opcode: when the ordering-table index is locked (a temporary
 * draw-area clip is active), end the clip by adding
 * g_world_gfx_temporary_draw_area (y reset to 0) one entry after the current
 * ordering-table index, and unlock the index. Returns the next instruction. */
menu_instruction_t* world_menu_opcode_13_clear_temporary_draw_area(menu_instruction_t* instruction) {
    if (g_gfx_otag_index_locked != 0) {
        g_world_gfx_temporary_draw_area.y = 0;
        world_gfx_add_draw_area_primitive(&g_world_gfx_temporary_draw_area, g_menu_gfx_otag_index + 1);
        g_gfx_otag_index_locked = 0;
    }
    return menu_next_instruction(instruction);
}

/* Menu-script opcode: unless the ordering-table index is locked, add the
 * instruction's draw area one entry before the current ordering-table index.
 * Returns the next instruction. */
menu_instruction_t* world_menu_opcode_14_set_draw_area(menu_instruction_t* instruction) {
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

/* Menu-script opcode: unless the ordering-table index is locked, add
 * g_world_gfx_temporary_draw_area (y reset to 0) one entry after the current
 * ordering-table index. Returns the next instruction. */
menu_instruction_t* world_menu_opcode_15_repeat_draw_area(menu_instruction_t* instruction) {
    if (g_gfx_otag_index_locked == 0) {
        g_world_gfx_temporary_draw_area.y = 0;
        world_gfx_add_draw_area_primitive(&g_world_gfx_temporary_draw_area, g_menu_gfx_otag_index + 1);
    }
    return menu_next_instruction(instruction);
}
