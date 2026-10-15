#include "fft/event_bunit.h"
#include "psx/types.h"

/* Draw rows of gouraud-textured quads whose vertex brightness falls off with
 * distance from the animated focal point. */
void bunit_gfx_draw_falloff_shaded_rows(s32 y) {
    textured_rect_source_t rect;
    /* Unused local bytes and RGB alignment retain the target frame. */
    u8 unused_frame[8];
    rgb24_t rgb[4];
    u8 shades[16][5];
    s32 row;
    s32 col;
    s32 brightness;
    s16 u;
    s32 i;

    row = 0;
    rect.w = 0x40;
    rect.clut = 0x40;
    rect.h = 0x20;
    rect.tpage = 0x20;
    rect.v = 0xD8;
    rect.y = y;
    for (i = 0; y < 0x101; y += 0x20, i++) {
        for (col = 0; col < 5; col++) {
            brightness = bunit_gfx_calculate_distance_falloff(col << 6, y, 0xFF, 0x50);
            if (brightness > 0xDC) {
                brightness = 0xDC;
            }
            shades[i][col] = brightness;
        }
    }

    rect.clut = g_bunit_gfx_background_clut;
    rect.tpage = g_bunit_menu_icon_texture_page;
    for (; rect.y < 0xF0; rect.y += 0x20, row++) {
        if (rect.y >= -0x20) {
            for (col = 0; col < 4; col++) {
                rect.x = g_bunit_gfx_background_tile_layout[row % 2][col * 2];
                u = g_bunit_gfx_background_tile_layout[row % 2][col * 2 + 1];
                rect.u = u;
                rgb[0].r = rgb[0].g = rgb[0].b = shades[row][col];
                rgb[1].r = rgb[1].g = rgb[1].b = shades[row][col + 1];
                rgb[2].r = rgb[2].g = rgb[2].b = shades[row + 1][col];
                rgb[3].r = rgb[3].g = rgb[3].b = shades[row + 1][col + 1];
                gfx_enqueue_gouraud_textured_quad(&rect, rgb, 0, u == 0x58);
            }
        }
    }
}
