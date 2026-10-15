#include "fft/world.h"

/* The incoming mode stays in a2 until the first drawing-helper call. */
void world_menu_draw_row_window_and_cursor(s32 wide, s32 row, s32 mode) {
    s16 x;
    s16 y;
    /* One pointer serves the script then the quad; separate locals remove the argument copy. */
    union {
        menu_instruction_t* script;
        oriented_quad_t* quad;
    } drawing;
    s32 cursor_x;
    oriented_quad_t* cursor;

    x = g_world_item_category_tab_x[row];
    if (wide != 0) {
        x -= 1;
    }
    if (wide == 0) {
        drawing.script = &g_world_item_category_tabs_script;
        y = 0x40;
    } else {
        y = 0x4F;
        drawing.script = &g_world_item_category_tabs_wide_script;
    }
    menu_run_script_with_palette_mode_variant(drawing.script, 0, mode);
    drawing.quad = &g_world_item_category_tab_sprites[row];
    drawing.quad->position.x = x;
    drawing.quad->position.y = y - (row == 0);
    menu_gfx_enqueue_oriented_textured_quad(
        drawing.quad, menu_gfx_get_draw_color_variant(), TEXTURE_UV_FLIP_NONE, 0, 0x2C);
    cursor = &g_world_item_category_tab_cursor_sprite;
    cursor_x = x + 9;
    if (row == 3) {
        cursor_x = x + 7;
    }
    cursor->position.x = cursor_x;
    g_world_item_category_tab_cursor_sprite.position.y = y + 10;
    menu_gfx_enqueue_oriented_textured_quad(cursor, 0, TEXTURE_UV_FLIP_NONE, 0, 0x2D);
}
