/* Menu window-frame commands and their opening transition. */

#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draw a menu window's border and tile its interior through the overlay's handler.
 * Source 1 selects alternate border UVs. The final command reassignment
 * preserves the original pointer allocation. */
menu_instruction_t* menu_opcode_00_draw_window_frame(menu_instruction_t* command) {
    RECT rect;
    s32 left;
    s32 top;
    s32 full_columns;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 priority = g_menu_gfx_otag_index;
    s16 u_left;
    s16 u_middle;
    s16 u_right;
    s16 v_bottom;
    u16 clut;
    s16 v_top;
    s32 column;
    s32 row;
    u8 width;
    u8 height;

    left = command->window.x - 5;
    top = command->window.y - 9;
    width = command->window.width;
    full_columns = width >> 4;
    partial_width = width & 15;
    height = command->window.height;
    full_rows = height >> 4;
    partial_height = height & 15;
    clut = g_menu_palette_clut;
    g_menu_window_interior_command.rectangle_source = command->window.rectangle_source;
    g_menu_window_right_x = command->window.width + left + 5;
    if (command->window.rectangle_source == 1) {
        u_left = 0xDA;
        v_top = 3;
        u_middle = 0xDF;
        u_right = 0xF7;
        v_bottom = 0x18;
    } else {
        u_left = 2;
        v_top = 2;
        u_middle = 8;
        u_right = 0x1F;
        v_bottom = 0x1C;
    }

    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, u_left, v_top, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, u_middle, v_top, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 9;
        menu_gfx_enqueue_textured_quad(
            &rect, u_middle, v_top, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, u_right - 5, v_top, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);

    rect.x = left;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    menu_gfx_enqueue_textured_quad(
        &rect, 2, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    rect.x = left + (5 + partial_width) + full_columns * 16;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    menu_gfx_enqueue_textured_quad(
        &rect, 0x1A, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    for (row = 0; row < full_rows; row++) {
        rect.x = left;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 2, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
        rect.x = left + (5 + partial_width) + full_columns * 16;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 0x1A, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    }

    g_menu_window_interior_command.x = command->window.x;
    g_menu_window_interior_command.y = command->window.y;
    g_menu_window_interior_command.width = command->window.width;
    g_menu_window_interior_command.height = command->window.height;
    menu_opcode_11_draw_background_tiles((menu_instruction_t*)&g_menu_window_interior_command);

    top = top + 9 + partial_height + full_rows * 16;
    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, u_left, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, u_middle, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 7;
        menu_gfx_enqueue_textured_quad(
            &rect, u_middle, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, u_right - 5, v_bottom - 7, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, priority);
    command = menu_next_instruction(command);
    return command;
}

/* Stream command for a window frame that zooms open: during the first four
 * transition frames it first queues a draw area cropped about the window's
 * center to the zoom table's percentage of its size (and, for the first
 * three, withholds the stream's input), then draws the frame through
 * menu_opcode_00_draw_window_frame. */
menu_instruction_t* menu_opcode_12_draw_opening_window_frame(menu_instruction_t* command) {
    RECT area;
    s32 frame = g_gfx_transition_frame;
    s32 scale;

    if (frame < 4) {
        if (frame < 3) {
            g_menu_script_input = 0;
        }
        scale = g_gfx_window_zoom_percent[frame];
        area.w = command->window.width * scale / 100;
        area.h = command->window.height * scale / 100;
        area.x = command->window.x + (command->window.width >> 1) - (area.w >> 1);
        area.y = command->window.y + (command->window.height >> 1) - (area.h >> 1);
        menu_gfx_enqueue_draw_area(&area, g_menu_gfx_otag_index - 1);
        g_gfx_otag_index_locked = 1;
    } else {
        g_gfx_otag_index_locked = 0;
    }
    return menu_opcode_00_draw_window_frame(command);
}
