#include "fft/event_card.h"
#include "psx/types.h"

world_menu_window_command_t* card_cmd_draw_window_frame_handler(world_menu_window_command_t* command) {
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

    priority = g_card_text_otag_index;
    clut = g_card_text_metric_2;
    if (command->rectangle_source == 2) {
        left = g_card_menu_window_rect_source->rect.x - 5;
        top = g_card_menu_window_rect_source->rect.y - 9;
        full_columns = g_card_menu_window_rect_source->rect.w / 16;
        partial_width = (s16)(g_card_menu_window_rect_source->rect.w % 16);
        full_rows = g_card_menu_window_rect_source->rect.h / 16;
        partial_height = (s16)(g_card_menu_window_rect_source->rect.h % 16);
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
    g_card_cmd_window_interior_command.rectangle_source = command->rectangle_source;
    g_card_menu_window_right_x = command->width + left + 5;
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
    card_gfx_enqueue_textured_quad(
        &rect, u_left, v_top, 0, g_card_gfx_draw_semitrans, g_card_text_digit_texture_page, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 9;
    card_gfx_enqueue_textured_quad(
        &rect, u_middle, v_top, 0, g_card_gfx_draw_semitrans, g_card_text_digit_texture_page, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 9;
        card_gfx_enqueue_textured_quad(
            &rect, u_middle, v_top, 0, g_card_gfx_draw_semitrans, g_card_text_digit_texture_page, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    card_gfx_enqueue_textured_quad(
        &rect, u_right - 5, v_top, 0, g_card_gfx_draw_semitrans, g_card_text_digit_texture_page, clut, priority);

    rect.x = left;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    card_gfx_enqueue_textured_quad(
        &rect, 2, 8, 0, g_card_gfx_draw_semitrans, g_card_text_digit_texture_page, clut, priority);
    rect.x = left + (5 + partial_width) + full_columns * 16;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    card_gfx_enqueue_textured_quad(
        &rect, 0x1A, 8, 0, g_card_gfx_draw_semitrans, g_card_text_digit_texture_page, clut, priority);
    for (row = 0; row < full_rows; row++) {
        rect.x = left;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        card_gfx_enqueue_textured_quad(
            &rect, 2, 8, 0, g_card_gfx_draw_semitrans, g_card_text_digit_texture_page, clut, priority);
        rect.x = left + (5 + partial_width) + full_columns * 16;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        card_gfx_enqueue_textured_quad(
            &rect, 0x1A, 8, 0, g_card_gfx_draw_semitrans, g_card_text_digit_texture_page, clut, priority);
    }

    if (command->rectangle_source == 2) {
        g_card_gfx_tiled_rect.x = g_card_menu_window_rect_source->rect.x;
        g_card_gfx_tiled_rect.y = g_card_menu_window_rect_source->rect.y;
        g_card_gfx_tiled_rect.w = g_card_menu_window_rect_source->rect.w;
        g_card_gfx_tiled_rect.h = g_card_menu_window_rect_source->rect.h;
    } else {
        g_card_cmd_window_interior_command.x = command->x;
        g_card_cmd_window_interior_command.y = command->y;
        g_card_cmd_window_interior_command.width = command->width;
        g_card_cmd_window_interior_command.height = command->height;
    }
    card_cmd_draw_tiled_rectangle_handler((u8*)&g_card_cmd_window_interior_command);

    top = top + 9 + partial_height + full_rows * 16;
    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    card_gfx_enqueue_textured_quad(
        &rect, u_left, v_bottom - 7, 0, g_card_gfx_draw_semitrans, g_card_text_digit_texture_page, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 7;
    card_gfx_enqueue_textured_quad(
        &rect, u_middle, v_bottom - 7, 0, g_card_gfx_draw_semitrans, g_card_text_digit_texture_page, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 7;
        card_gfx_enqueue_textured_quad(&rect, u_middle, v_bottom - 7, 0, g_card_gfx_draw_semitrans,
            g_card_text_digit_texture_page, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    card_gfx_enqueue_textured_quad(
        &rect, u_right - 5, v_bottom - 7, 0, g_card_gfx_draw_semitrans, g_card_text_digit_texture_page, clut, priority);
    command = (world_menu_window_command_t*)((u8*)command + command->length);
    return command;
}
