#include "fft/world.h"

/*
 * Draw a menu window's 5x9/5x7 border pieces around its rectangle and tile
 * the interior through world_menu_parse_draw_tiled_rectangle.
 *
 * Source 2 takes the signed rectangle from the shared window record; other
 * sources use the command's byte operands. Source 1 selects the alternate
 * border texture coordinates. The copied return statement keeps the command
 * pointer's reference count above the draw priority's, so global allocation
 * places them in s6 and s7 like the target.
 */
world_menu_window_command_t* world_menu_parse_draw_window_frame(world_menu_window_command_t* command) {
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

    priority = g_world_menu_draw_priority;
    clut = g_world_menu_palette_clut;
    if (command->rectangle_source == 2) {
        left = g_world_menu_window_rect_source->rect.x - 5;
        top = g_world_menu_window_rect_source->rect.y - 9;
        full_columns = g_world_menu_window_rect_source->rect.w / 16;
        partial_width = (s16)(g_world_menu_window_rect_source->rect.w % 16);
        full_rows = g_world_menu_window_rect_source->rect.h / 16;
        partial_height = (s16)(g_world_menu_window_rect_source->rect.h % 16);
    } else {
        u8 width;
        u8 height;
        left = command->x - 5;
        top = command->y - 9;
        width = command->width;
        full_columns = width >> 4;
        partial_width = width & 15;
        height = command->height;
        full_rows = height >> 4;
        partial_height = height & 15;
    }
    g_world_menu_window_interior_command.rectangle_source = command->rectangle_source;
    g_world_menu_window_right_x = command->width + left + 5;
    v_top = 2;
    if (command->rectangle_source == 1) {
        u_left = 0xDA;
        v_top = 3;
        u_middle = 0xDF;
        u_right = 0xF7;
        v_bottom = 0x18;
    } else {
        u_left = 2;
        u_middle = 8;
        u_right = 0x1F;
        v_bottom = 0x1C;
    }

    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    world_gfx_enqueue_textured_quad(
        &rect, u_left, v_top, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 9;
    world_gfx_enqueue_textured_quad(
        &rect, u_middle, v_top, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 9;
        world_gfx_enqueue_textured_quad(
            &rect, u_middle, v_top, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    world_gfx_enqueue_textured_quad(
        &rect, u_right - 5, v_top, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);

    rect.x = left;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    world_gfx_enqueue_textured_quad(&rect, 2, 8, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
    rect.x = left + (5 + partial_width) + full_columns * 16;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    world_gfx_enqueue_textured_quad(
        &rect, 0x1A, 8, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
    for (row = 0; row < full_rows; row++) {
        rect.x = left;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        world_gfx_enqueue_textured_quad(
            &rect, 2, 8, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
        rect.x = left + (5 + partial_width) + full_columns * 16;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        world_gfx_enqueue_textured_quad(
            &rect, 0x1A, 8, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
    }

    if (command->rectangle_source == 2) {
        g_world_menu_window_rect.x = g_world_menu_window_rect_source->rect.x;
        g_world_menu_window_rect.y = g_world_menu_window_rect_source->rect.y;
        g_world_menu_window_rect.w = g_world_menu_window_rect_source->rect.w;
        g_world_menu_window_rect.h = g_world_menu_window_rect_source->rect.h;
    } else {
        g_world_menu_window_interior_command.x = command->x;
        g_world_menu_window_interior_command.y = command->y;
        g_world_menu_window_interior_command.width = command->width;
        g_world_menu_window_interior_command.height = command->height;
    }
    world_menu_parse_draw_tiled_rectangle(&g_world_menu_window_interior_command);

    top = top + 9 + partial_height + full_rows * 16;
    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    world_gfx_enqueue_textured_quad(
        &rect, u_left, v_bottom - 7, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 7;
    world_gfx_enqueue_textured_quad(
        &rect, u_middle, v_bottom - 7, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 7;
        world_gfx_enqueue_textured_quad(
            &rect, u_middle, v_bottom - 7, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    world_gfx_enqueue_textured_quad(
        &rect, u_right - 5, v_bottom - 7, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
    command = (world_menu_window_command_t*)((u8*)command + command->length);
    return command;
}
