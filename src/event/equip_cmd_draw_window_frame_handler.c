#include "fft/equip.h"
#include "psx/types.h"

/* Draw a menu window's 5x9/5x7 border pieces around the command's rectangle
 * and tile its interior through equip_cmd_draw_tiled_rectangle_handler.
 *
 * EQUIP twin of WORLD world_menu_parse_draw_window_frame without its
 * shared-rectangle source; source 1 selects the alternate border texture
 * coordinates. The reassigned command pointer before the return keeps its
 * reference count high enough for global allocation to place it in s5. */
equip_menu_window_command_t* equip_cmd_draw_window_frame_handler(equip_menu_window_command_t* command) {
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

    priority = g_equip_gfx_sprite_ot_index;
    left = command->x - 5;
    top = command->y - 9;
    width = command->width;
    full_columns = width >> 4;
    partial_width = width & 15;
    height = command->height;
    full_rows = height >> 4;
    partial_height = height & 15;
    clut = g_equip_text_metric_2;
    g_equip_cmd_window_interior_command.rectangle_source = command->rectangle_source;
    g_equip_menu_window_right_x = command->width + left + 5;
    if (command->rectangle_source == 1) {
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
    equip_gfx_enqueue_textured_quad(
        &rect, u_left, v_top, 0, g_equip_gfx_semitransparency, g_equip_text_digit_texture_page, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 9;
    equip_gfx_enqueue_textured_quad(
        &rect, u_middle, v_top, 0, g_equip_gfx_semitransparency, g_equip_text_digit_texture_page, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 9;
        equip_gfx_enqueue_textured_quad(
            &rect, u_middle, v_top, 0, g_equip_gfx_semitransparency, g_equip_text_digit_texture_page, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 9;
    equip_gfx_enqueue_textured_quad(
        &rect, u_right - 5, v_top, 0, g_equip_gfx_semitransparency, g_equip_text_digit_texture_page, clut, priority);

    rect.x = left;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    equip_gfx_enqueue_textured_quad(
        &rect, 2, 8, 0, g_equip_gfx_semitransparency, g_equip_text_digit_texture_page, clut, priority);
    rect.x = left + (5 + partial_width) + full_columns * 16;
    rect.y = top + 9;
    rect.w = 5;
    rect.h = partial_height;
    equip_gfx_enqueue_textured_quad(
        &rect, 0x1A, 8, 0, g_equip_gfx_semitransparency, g_equip_text_digit_texture_page, clut, priority);
    for (row = 0; row < full_rows; row++) {
        rect.x = left;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        equip_gfx_enqueue_textured_quad(
            &rect, 2, 8, 0, g_equip_gfx_semitransparency, g_equip_text_digit_texture_page, clut, priority);
        rect.x = left + (5 + partial_width) + full_columns * 16;
        rect.y = top + (9 + partial_height) + row * 16;
        rect.w = 5;
        rect.h = 16;
        equip_gfx_enqueue_textured_quad(
            &rect, 0x1A, 8, 0, g_equip_gfx_semitransparency, g_equip_text_digit_texture_page, clut, priority);
    }

    g_equip_cmd_window_interior_command.x = command->x;
    g_equip_cmd_window_interior_command.y = command->y;
    g_equip_cmd_window_interior_command.width = command->width;
    g_equip_cmd_window_interior_command.height = command->height;
    equip_cmd_draw_tiled_rectangle_handler((u8*)&g_equip_cmd_window_interior_command);

    top = top + 9 + partial_height + full_rows * 16;
    rect.x = left;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    equip_gfx_enqueue_textured_quad(
        &rect, u_left, v_bottom - 7, 0, g_equip_gfx_semitransparency, g_equip_text_digit_texture_page, clut, priority);
    rect.x = left + 5;
    rect.y = top;
    rect.w = partial_width;
    rect.h = 7;
    equip_gfx_enqueue_textured_quad(&rect, u_middle, v_bottom - 7, 0, g_equip_gfx_semitransparency,
        g_equip_text_digit_texture_page, clut, priority);
    for (column = 0; column < full_columns; column++) {
        rect.x = left + (5 + partial_width) + column * 16;
        rect.y = top;
        rect.w = 16;
        rect.h = 7;
        equip_gfx_enqueue_textured_quad(&rect, u_middle, v_bottom - 7, 0, g_equip_gfx_semitransparency,
            g_equip_text_digit_texture_page, clut, priority);
    }
    rect.x = left + (5 + partial_width) + column * 16;
    rect.y = top;
    rect.w = 5;
    rect.h = 7;
    equip_gfx_enqueue_textured_quad(&rect, u_right - 5, v_bottom - 7, 0, g_equip_gfx_semitransparency,
        g_equip_text_digit_texture_page, clut, priority);
    command = (equip_menu_window_command_t*)((u8*)command + command->length);
    return command;
}
