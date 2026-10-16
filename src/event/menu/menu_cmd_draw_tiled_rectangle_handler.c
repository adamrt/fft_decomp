#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Tile the command rectangle, or the shared rectangle when source is 2. */
menu_window_command_t* menu_cmd_draw_tiled_rectangle_handler(menu_window_command_t* command) {
    RECT rect;
    s32 base_x;
    s32 base_y;
    s32 full_rows;
    s32 partial_width;
    s32 partial_height;
    s32 otag_index = g_menu_gfx_otag_index;
    u16 clut = g_menu_palette_clut;
    s32 full_columns;
    s32 row;
    s32 column;

    if (command->rectangle_source == 2) {
        base_x = g_gfx_tiled_rect.x;
        base_y = g_gfx_tiled_rect.y;
        full_columns = g_gfx_tiled_rect.w / 16;
        partial_width = g_gfx_tiled_rect.w % 16;
        full_rows = g_gfx_tiled_rect.h / 16;
        partial_height = g_gfx_tiled_rect.h % 16;
    } else {
        u8 packed_width;
        u8 packed_height;

        base_x = command->x;
        base_y = command->y;
        packed_width = command->width;
        packed_height = command->height;
        full_columns = packed_width >> 4;
        partial_width = packed_width & 15;
        full_rows = packed_height >> 4;
        partial_height = packed_height & 15;
    }

    for (row = 0; row < full_rows; row++) {
        rect.x = base_x;
        rect.y = base_y + row * 16;
        rect.w = partial_width;
        rect.h = 16;
        menu_gfx_enqueue_textured_quad(
            &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, otag_index);
        for (column = 0; column < full_columns; column++) {
            rect.x = base_x + partial_width + column * 16;
            rect.y = base_y + row * 16;
            rect.w = 16;
            rect.h = 16;
            menu_gfx_enqueue_textured_quad(
                &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, otag_index);
        }
    }

    if (partial_height != 0) {
        rect.x = base_x;
        rect.y = base_y + row * 16;
        rect.w = partial_width;
        rect.h = partial_height;
        menu_gfx_enqueue_textured_quad(
            &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, otag_index);
        for (column = 0; column < full_columns; column++) {
            rect.x = base_x + partial_width + column * 16;
            rect.y = base_y + row * 16;
            rect.w = 16;
            rect.h = partial_height;
            menu_gfx_enqueue_textured_quad(
                &rect, 8, 8, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, otag_index);
        }
    }

    command = (menu_window_command_t*)((u8*)command + command->length);
    return command;
}
