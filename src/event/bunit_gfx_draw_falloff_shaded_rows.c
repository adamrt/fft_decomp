#include "fft/event_bunit.h"
#include "psx/types.h"

/* One stack frame block: the quad rectangle at sp+0x10, its twelve vertex
 * colour bytes at sp+0x28 and the brightness grid at sp+0x38 (five samples
 * per 32-pixel row). */
typedef struct {
    bunit_gouraud_rect_t rect; /* 0x00 */
    u8 unk_10[8];              /* 0x10 */
    u8 colors[16];             /* 0x18 */
    u8 brightness[16][5];      /* 0x28 */
} bunit_gfx_shaded_background_work_t;

/* Draw rows of gouraud-textured quads whose vertex brightness falls off with
 * distance from the animated focal point.
 *
 * The column loop is a backward goto inside a run-once block: gcc 2.6.3's
 * loop optimizer treats that block as a phony loop, so the per-column brightness
 * addresses stay unreduced as in the target, while the extra loop nesting
 * gives the reference weights that decide the target's register choice. */
void bunit_gfx_draw_falloff_shaded_rows(s32 y) {
    bunit_gfx_shaded_background_work_t work;
    s32 row;
    s32 col;
    s32 brightness;
    u16* layout;
    u16* layouts;
    s16 u;
    u16 row_y;
    s32 i;

    row = 0;
    work.rect.w = 0x40;
    work.rect.clut = 0x40;
    work.rect.h = 0x20;
    work.rect.tpage = 0x20;
    work.rect.v = 0xD8;
    work.rect.y = y;
    for (i = 0; y < 0x101; y += 0x20, i++) {
        for (col = 0; col < 5; col++) {
            brightness = bunit_gfx_calculate_distance_falloff(col << 6, y, 0xFF, 0x50);
            if (brightness > 0xDC) {
                brightness = 0xDC;
            }
            work.brightness[i][col] = brightness;
        }
    }

    work.rect.clut = g_bunit_gfx_background_clut;
    work.rect.tpage = g_bunit_menu_icon_texture_page;
    row_y = work.rect.y;
    if ((s16)work.rect.y < 0xF0) {
        do {
            if ((s16)row_y >= -0x20) {
                col = 0;
                do {
                    layouts = g_bunit_gfx_background_tile_layout;
                    layout = layouts + (row % 2) * 8;
                next_col:
                    work.rect.x = layout[0];
                    u = layout[1];
                    layout += 2;
                    work.rect.u = u;
                    work.colors[0] = work.colors[1] = work.colors[2] = work.brightness[row][col];
                    work.colors[3] = work.colors[4] = work.colors[5] = work.brightness[row][col + 1];
                    work.colors[6] = work.colors[7] = work.colors[8] = work.brightness[row + 1][col];
                    work.colors[9] = work.colors[10] = work.colors[11] = work.brightness[row + 1][col + 1];
                    bunit_gfx_enqueue_gouraud_textured_quad(&work.rect, work.colors, 0, u == 0x58);
                    if (++col < 4) {
                        goto next_col;
                    }
                } while (0);
            }
            work.rect.y += 0x20;
            row_y = work.rect.y;
            row++;
        } while ((s16)row_y < 0xF0);
    }
}
