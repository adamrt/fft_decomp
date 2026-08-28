#include "fft/world.h"

/* The incoming mode stays in a2 until the first drawing-helper call. */
void world_menu_draw_row_window_and_cursor(s32 wide, s32 row, s32 mode) {
    s16 x;
    s16 width;
    s16* rect;
    s32 cursor_x;
    world_oriented_sprite_t* cursor;

    x = g_world_item_category_tab_x[row];
    if (wide != 0) {
        x -= 1;
    }
    if (wide == 0) {
        rect = &g_world_item_category_tabs_script;
        width = 0x40;
    } else {
        width = 0x4F;
        rect = &g_world_item_category_tabs_wide_script;
    }
    world_menu_run_script_with_palette_mode(rect, 0, mode);
    rect = g_world_item_category_tab_sprites[row];
    rect[0] = x;
    rect[1] = width - (row == 0);
    world_gfx_enqueue_oriented_textured_quad(
        (const world_oriented_quad_t*)rect, (const u8*)world_menu_get_sprite_color(), 0, 0, 0x2C);
    cursor = &g_world_item_category_tab_cursor_sprite;
    cursor_x = x + 9;
    if (row == 3) {
        cursor_x = x + 7;
    }
    cursor->x = cursor_x;
    g_world_item_category_tab_cursor_sprite.y = width + 10;
    world_gfx_enqueue_oriented_textured_quad((const world_oriented_quad_t*)cursor, 0, 0, 0, 0x2D);
}
