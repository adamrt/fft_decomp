#include "fft/world.h"

/*
 * Tile a menu rectangle with 16-pixel sprites and clipped edge pieces.
 *
 * Source 2 uses the signed shared rectangle; other sources use byte operands.
 */
world_menu_window_command_t* world_menu_parse_draw_tiled_rectangle(world_menu_window_command_t* input) {
    /* Preserve the command in s7 so draw priority retains its reload slot. */
    register world_menu_window_command_t* command __asm__("$23") = input;
    RECT rect;
    s32 base_x;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 priority;
    u16 clut;
    s32 full_columns;
    s32 row;
    s32 column;
    s32 base_y;

    priority = g_world_menu_draw_priority;
    clut = g_world_menu_palette_clut;
    if (command->rectangle_source == 2) {
        base_x = g_world_menu_window_rect.x;
        base_y = g_world_menu_window_rect.y;
        full_columns = g_world_menu_window_rect.w / 16;
        partial_width = (s16)(g_world_menu_window_rect.w % 16);
        full_rows = g_world_menu_window_rect.h / 16;
        partial_height = (s16)(g_world_menu_window_rect.h % 16);
    } else {
        u8 width;
        u8 height;
        base_x = command->x;
        base_y = command->y;
        width = command->width;
        full_columns = width >> 4;
        partial_width = width & 15;
        height = command->height;
        full_rows = height >> 4;
        partial_height = height & 15;
    }
    for (row = 0; row < full_rows; row++) {
        rect.x = base_x;
        rect.y = base_y + row * 16;
        rect.w = partial_width;
        rect.h = 16;
        world_gfx_enqueue_textured_quad(
            &rect, 8, 8, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
        for (column = 0; column < full_columns; column++) {
            rect.x = base_x + partial_width + column * 16;
            rect.y = base_y + row * 16;
            rect.w = 16;
            rect.h = 16;
            world_gfx_enqueue_textured_quad(
                &rect, 8, 8, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
        }
    }
    if (partial_height != 0) {
        rect.x = base_x;
        rect.y = base_y + row * 16;
        rect.w = partial_width;
        rect.h = partial_height;
        world_gfx_enqueue_textured_quad(
            &rect, 8, 8, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
        for (column = 0; column < full_columns; column++) {
            rect.x = base_x + partial_width + column * 16;
            rect.y = base_y + row * 16;
            rect.w = 16;
            rect.h = partial_height;
            world_gfx_enqueue_textured_quad(
                &rect, 8, 8, 0, g_world_menu_semi_trans, g_world_menu_window_tpage, clut, priority);
        }
    }
    return (world_menu_window_command_t*)((u8*)command + command->length);
}
