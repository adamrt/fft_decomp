#include "fft/menu.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Builds the twelve-line menu box primitives for `box` (x, y, w, h) into
 * `menu`: line endpoints from the layout table, palette-bank-0 colours,
 * two draw modes and a sprite covering the box. */
void world_menu_build_line_box(RECT* box, world_menu_palette_primitives_t* menu) {
    LINE_F2* line;
    s32 i;
    s32 offset;

    line = menu->lines;
    for (i = 0; i < 12; i++) {
        SetLineF2(line);
        line++;
    }
    world_menu_init_primitive_colors_palette_bank_0(menu);
    line = menu->lines;
    offset = 0;
    for (i = 0; i < 12; i++) {
        line->x0 = g_world_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_X0] + box->x;
        line->y0 = g_world_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_Y0] + box->y;
        line->x1 = g_world_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_X1] + box->x;
        line->y1 = g_world_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_Y1] + box->y;
        if (g_world_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_ADD_W0] != 0) {
            line->x0 += box->w;
        }
        if (g_world_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_ADD_H0] != 0) {
            line->y0 += box->h;
        }
        if (g_world_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_ADD_W1] != 0) {
            line->x1 += box->w;
        }
        if (g_world_menu_numeric_display_frame_offsets[offset + WORLD_MENU_LINE_LAYOUT_ADD_H1] != 0) {
            line->y1 += box->h;
        }
        offset += WORLD_MENU_LINE_LAYOUT_SIZE;
        line++;
    }
    SetDrawMode(
        &menu->draw_mode_menu, 1, 0, (u16)GetTPage(0, 0, 0x3c0, 0x100), &g_world_menu_numeric_display_texture_window);
    SetDrawMode(&menu->draw_mode, 0, 0, (u16)GetTPage(0, 2, 0x3c0, 0x100), &g_world_gfx_texture_window);
    world_menu_init_sprite(&menu->sprite);
    menu->sprite.x0 = box->x;
    menu->sprite.y0 = box->y;
    menu->sprite.w = box->w;
    menu->sprite.h = box->h;
}
