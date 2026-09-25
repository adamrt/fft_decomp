#include "fft/jobstts.h"

/* Draw a window's 5x9/5x7 border pieces around its rectangle and tile the
 * interior through jobstts_cmd_draw_background_tiles_handler.
 *
 * JOBSTTS copy of world_menu_parse_draw_window_frame without the rectangle
 * source selection. Reassigning the command pointer before the return keeps
 * its reference count high enough for global allocation to place it in s6.
 */

u8* jobstts_create_border_tiles(u8* stream) {
    world_menu_window_command_t* command;
    urect16_t rect;
    s32 left;
    s32 top;
    s32 full_columns;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 priority;
    u16 clut;
    s32 column;
    s32 row;
    u8 width;
    u8 height;

    command = (world_menu_window_command_t*)stream;
    priority = g_jobstts_gfx_otag_index;
    clut = g_jobstts_gfx_background_clut_id;
    left = command->x - 5;
    top = command->y - 9;
    width = command->width;
    full_columns = width >> 4;
    partial_width = width & 15;
    height = command->height;
    full_rows = height >> 4;
    partial_height = height & 15;
    g_jobstts_menu_window_right_x = command->width + left + 5;

    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    jobstts_gfx_enqueue_textured_quad(
        &rect, 0xDA, 3, 0, g_jobstts_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 9;
    jobstts_gfx_enqueue_textured_quad(
        &rect, 0xDF, 3, 0, g_jobstts_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 9;
        jobstts_gfx_enqueue_textured_quad(
            &rect, 0xDF, 3, 0, g_jobstts_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    jobstts_gfx_enqueue_textured_quad(
        &rect, 0xF2, 3, 0, g_jobstts_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);

    rect.x = left;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    jobstts_gfx_enqueue_textured_quad(
        &rect, 2, 8, 0, g_jobstts_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    rect.x = left + (5 + partial_width) + full_columns * 16;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    jobstts_gfx_enqueue_textured_quad(
        &rect, 0x1A, 8, 0, g_jobstts_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    for (row = 0; row < full_rows; row++) {
        rect.x = left;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        jobstts_gfx_enqueue_textured_quad(
            &rect, 2, 8, 0, g_jobstts_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
        rect.x = left + (5 + partial_width) + full_columns * 16;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        jobstts_gfx_enqueue_textured_quad(
            &rect, 0x1A, 8, 0, g_jobstts_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    }

    g_jobstts_cmd_window_interior_command.x = command->x;
    g_jobstts_cmd_window_interior_command.y = command->y;
    g_jobstts_cmd_window_interior_command.width = command->width;
    g_jobstts_cmd_window_interior_command.height = command->height;
    jobstts_cmd_draw_background_tiles_handler((u8*)&g_jobstts_cmd_window_interior_command);

    top = top + 9 + partial_height + full_rows * 16;
    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    jobstts_gfx_enqueue_textured_quad(
        &rect, 0xDA, 0x11, 0, g_jobstts_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 7;
    jobstts_gfx_enqueue_textured_quad(
        &rect, 0xDF, 0x11, 0, g_jobstts_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 7;
        jobstts_gfx_enqueue_textured_quad(&rect, 0xDF, 0x11, 0, g_jobstts_gfx_semitransparency,
            g_jobstts_gfx_background_texture_page, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    jobstts_gfx_enqueue_textured_quad(
        &rect, 0xF2, 0x11, 0, g_jobstts_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, priority);
    command = (world_menu_window_command_t*)((u8*)command + command->length);
    return (u8*)command;
}
