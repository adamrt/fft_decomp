#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draw a menu window's border and tile its interior through the overlay's handler.
 * Source 1 selects alternate border UVs. The final command reassignment
 * preserves the original pointer allocation. */
menu_instruction_t* menu_cmd_draw_window_frame_handler(menu_instruction_t* command) {
    RECT rect;
    s32 left;
    s32 top;
    s32 full_columns;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 priority;
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

    priority = g_menu_gfx_otag_index;
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
    menu_cmd_draw_background_tiles_handler((menu_instruction_t*)&g_menu_window_interior_command);

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
