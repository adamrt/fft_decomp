#include "fft/world.h"
#include "psx/types.h"

/*
 * Draw the formation-screen background as 64x32 tiles from `y` downwards,
 * shading each tile corner by its distance from the formation cursor.
 *
 * The target reads the rectangle's y with `lhu` and sign-extends it for each
 * comparison, hence the `(s16)(u16)` reads. `draw_row` is bound to s4 because
 * the target recomputes `draw_row * 5` inside the tile loop; the loop
 * optimizer never treats a hard register as invariant, while a pseudo lets it
 * hoist that product and the shade-row addresses.
 */
void world_formation_draw_background_tiles(s32 y, s32 unused_1) {
    textured_rect_source_t rect;
    /* Unused local bytes and RGB alignment retain the target frame. */
    u8 unused_frame[8];
    rgb24_t rgb[4];
    u8 shades[16][5];
    s32 row;
    s32 col;
    register s32 draw_row __asm__("$20");
    s32 shade;
    world_formation_background_tile_t* tile;
    world_formation_background_tile_t(*tiles)[4];

    draw_row = 0;
    rect.y = y;
    rect.w = 0x40;
    rect.clut = 0x40;
    rect.h = 0x20;
    rect.tpage = 0x20;
    rect.v = 0xD8;
    for (row = 0; y < 0x101; row++) {
        for (col = 0; col < 5; col++) {
            shade = world_formation_cursor_distance_falloff(col << 6, y, 0xFF, 0x50);
            shades[row][col] = (shade >= 0xDD) ? 0xDC : shade;
        }
        y += 0x20;
    }
    rect.clut = g_world_menu_background_clut;
    rect.tpage = g_menu_cursor_texture_page;
    for (; rect.y < 0xF0; draw_row++) {
        tiles = g_world_formation_background_tiles;
        if (rect.y >= -0x20) {
            col = 0;
            tile = tiles[draw_row % 2];
            for (; col < 4; col++) {
                rect.x = tile[col].x;
                rect.u = tile[col].u;
                rgb[0].r = rgb[0].g = rgb[0].b = shades[draw_row][col];
                rgb[1].r = rgb[1].g = rgb[1].b = shades[draw_row][col + 1];
                rgb[2].r = rgb[2].g = rgb[2].b = shades[draw_row + 1][col];
                rgb[3].r = rgb[3].g = rgb[3].b = shades[draw_row + 1][col + 1];
                gfx_enqueue_gouraud_textured_quad(&rect, rgb, 0, rect.u == 0x58);
            }
        }
        rect.y += 0x20;
    }
}
