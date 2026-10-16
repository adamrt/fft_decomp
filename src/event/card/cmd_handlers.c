/* The textured quad and window frame command handlers. */

#include "fft/event_card.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Stream command: draw a textured quad of size (command[5], command[6]) at
 * (command[3], command[4]) with texture origin (command[7], command[8]), in the
 * current sprite color or, for MENU_SCRIPT_OP_UNCOLORED_QUAD, unshaded. Returns the next command. */
u8* card_cmd_draw_textured_quad_handler(u8* command) {
    RECT rect;
    const rgb24_t* color;
    s32 x;
    s32 y;

    x = command[3];
    y = command[4];
    rect.x = x;
    rect.y = y;
    rect.w = command[5];
    rect.h = command[6];
    color = &g_menu_text_color;
    if (command[0] == MENU_SCRIPT_OP_UNCOLORED_QUAD) {
        color = 0;
    }
    menu_gfx_enqueue_textured_quad(&rect, command[7], command[8], color, g_menu_gfx_semitransparency,
        g_card_gfx_texture_page, g_card_gfx_draw_clut, g_menu_gfx_otag_index);
    return command + command[1];
}

/*
 * Draw a menu window's 5x9/5x7 border pieces around its rectangle and tile
 * the interior through menu_cmd_draw_tiled_rectangle_handler.
 *
 * CARD copy of world_menu_parse_draw_window_frame: source 2 takes the signed
 * rectangle from the shared window record, other sources use the command's
 * byte operands, and source 1 selects the alternate border texture
 * coordinates. Returns the command that follows.
 */
menu_window_command_t* card_cmd_draw_window_frame_handler(menu_window_command_t* command) {
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

    priority = g_menu_gfx_otag_index;
    clut = g_menu_palette_clut;
    if (command->rectangle_source == 2) {
        left = g_menu_window_rect_source->rect.x - 5;
        top = g_menu_window_rect_source->rect.y - 9;
        full_columns = g_menu_window_rect_source->rect.w / 16;
        partial_width = g_menu_window_rect_source->rect.w % 16;
        full_rows = g_menu_window_rect_source->rect.h / 16;
        partial_height = g_menu_window_rect_source->rect.h % 16;
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
    g_cmd_window_interior_command.rectangle_source = command->rectangle_source;
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

    if (command->rectangle_source == 2) {
        g_gfx_tiled_rect.x = g_menu_window_rect_source->rect.x;
        g_gfx_tiled_rect.y = g_menu_window_rect_source->rect.y;
        g_gfx_tiled_rect.w = g_menu_window_rect_source->rect.w;
        g_gfx_tiled_rect.h = g_menu_window_rect_source->rect.h;
    } else {
        g_cmd_window_interior_command.x = command->x;
        g_cmd_window_interior_command.y = command->y;
        g_cmd_window_interior_command.width = command->width;
        g_cmd_window_interior_command.height = command->height;
    }
    menu_cmd_draw_tiled_rectangle_handler(&g_cmd_window_interior_command);

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
    command = (menu_window_command_t*)((u8*)command + command->length);
    return command;
}
