/* The window frame command handler and border tiles with a transition clip. */

#include "fft/event_jobstts.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draw a window's 5x9/5x7 border pieces around its rectangle and tile the
 * interior through menu_cmd_draw_background_tiles_handler.
 *
 * JOBSTTS copy of world_menu_parse_draw_window_frame without the rectangle
 * source selection. Reassigning the command pointer before the return keeps
 * its reference count high enough for global allocation to place it in s6.
 */
menu_instruction_t* jobstts_cmd_draw_window_frame_handler(menu_instruction_t* command) {
    RECT rect;
    s32 left;
    s32 top;
    s32 full_columns;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 priority = g_menu_gfx_otag_index;
    u16 clut = g_menu_palette_clut;
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
    g_jobstts_menu_window_right_x = command->window.width + left + 5;

    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, 0xDA, 3, 0, g_menu_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, 0xDF, 3, 0, g_menu_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 9;
        menu_gfx_enqueue_textured_quad(
            &rect, 0xDF, 3, 0, g_menu_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    menu_gfx_enqueue_textured_quad(
        &rect, 0xF2, 3, 0, g_menu_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);

    rect.x = left;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    menu_gfx_enqueue_textured_quad(
        &rect, 2, 8, 0, g_menu_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    rect.x = left + (5 + partial_width) + full_columns * 16;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    menu_gfx_enqueue_textured_quad(
        &rect, 0x1A, 8, 0, g_menu_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    for (row = 0; row < full_rows; row++) {
        rect.x = left;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 2, 8, 0, g_menu_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
        rect.x = left + (5 + partial_width) + full_columns * 16;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 0x1A, 8, 0, g_menu_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    }

    g_cmd_window_interior_command.x = command->window.x;
    g_cmd_window_interior_command.y = command->window.y;
    g_cmd_window_interior_command.width = command->window.width;
    g_cmd_window_interior_command.height = command->window.height;
    menu_cmd_draw_background_tiles_handler((menu_instruction_t*)&g_cmd_window_interior_command);

    top = top + 9 + partial_height + full_rows * 16;
    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, 0xDA, 0x11, 0, g_menu_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, 0xDF, 0x11, 0, g_menu_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 7;
        menu_gfx_enqueue_textured_quad(
            &rect, 0xDF, 0x11, 0, g_menu_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    menu_gfx_enqueue_textured_quad(
        &rect, 0xF2, 0x11, 0, g_menu_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    command = menu_next_instruction(command);
    return command;
}

/* Draw the border tiles through a vertically expanding transition clip. */
menu_instruction_t* jobstts_menu_cmd_draw_border_tiles_with_transition_clip(menu_instruction_t* command) {
    menu_instruction_t* next;
    RECT rect;

    if (g_gfx_transition_frame < 8) {
        s32 scale;

        if (g_gfx_transition_frame < 3) {
            g_menu_cmd_stream_input = 0;
        }
        scale = g_jobstts_gfx_transition_height_percent[g_gfx_transition_frame];
        rect.w = 0x100;
        rect.h = (command->window.height * scale) / 100;
        rect.x = 0;
        rect.y = (command->window.y + (command->window.height >> 1)) - (rect.h >> 1);
        menu_gfx_enqueue_draw_area(&rect, g_menu_gfx_otag_index);
        g_gfx_otag_index_locked = 1;
        g_menu_gfx_otag_index = g_menu_gfx_otag_index + 1;
    } else {
        g_gfx_otag_index_locked = 0;
    }
    next = jobstts_cmd_draw_window_frame_handler(command);
    g_menu_gfx_otag_index++;
    return next;
}
