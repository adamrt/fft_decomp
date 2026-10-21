/* Load the selected map title and animate its shaded strips after deployment
 * while the battle map finishes loading. */

#include "fft/battle_map.h"
#include "fft/file.h"
#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/script_variable.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Map-title texture and its 16-color palette, transferred as eight GPU words. */
extern RECT g_battle_map_title_image_rect;
extern u32 g_battle_map_title_palette_data[8];
extern RECT g_battle_map_title_palette_rect;
static s32 s_battle_map_title_closing_past_midpoint;

/* Two buffers of the four shaded title-panel strips. */
extern POLY_GT4 g_battle_map_title_sprites[2][4];

/*
 * Builds and submits a 2x2 set of shaded POLY_GT4 panel strips at the current
 * thread's position.
 *
 * Row 0 is the solid part of the strip and row 1 a narrow edge whose shade
 * fades to 0; `reverse` selects which side of `width` is drawn. Column 0 is
 * the same quad offset by one pixel with a different blend mode.
 *
 * `height` is a local rather than a literal so the narrowed y2/y3 sums reuse
 * the y0 sum (a literal lets fold reassociate them). `rest` is assigned inside
 * its branch: that keeps the target's loop hoisting, where
 * `x + (0x80 + width)` is recomputed once per row instead of leaving both loops.
 */
static void battle_map_title_build_strips(POLY_GT4* polys, s32 width, s32 shade, s32 reverse) {
    POLY_GT4* poly;
    s32 row;
    s32 rest;
    s32 x = thread_current_parameter_1_get();
    s32 y = thread_current_parameter_2_get();
    s32 col;
    s32 dx;
    s32 dy;
    s32 edge;
    s32 height;

    poly = polys;
    height = 0x14;
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 2; col++) {
            SetPolyGT4(poly);
            SetShadeTex(poly, 0);
            SetSemiTrans(poly, 1);
            if (col != 0) {
                if (GetGraphType() == 1 || GetGraphType() == 2) {
                    poly->tpage = 0x87;
                } else {
                    poly->tpage = 0x27;
                }
                dx = 0;
                dy = 0;
            } else {
                if (GetGraphType() == 1 || GetGraphType() == 2) {
                    poly->tpage = 0x107;
                } else {
                    poly->tpage = 0x47;
                }
                dx = 1;
                dy = 1;
            }
            poly->clut = 0x3fdc;
            if (row == 0) {
                if (reverse == 0) {
                    setUVWH(poly, 0, 0xe0, width, 0x14);
                    setXYWH(poly, x + 0x80 + dx, y + 0x60 + dy, width, height);
                } else {
                    rest = 0xff - width;
                    setUVWH(poly, width, 0xe0, rest, 0x14);
                    setXYWH(poly, x + (0x80 + width) + dx, y + 0x60 + dy, rest, height);
                }
                setRGB0(poly, shade, shade, shade);
                setRGB1(poly, shade, shade, shade);
                setRGB2(poly, shade, shade, shade);
                setRGB3(poly, shade, shade, shade);
            } else {
                if (reverse == 0) {
                    setRGB0(poly, shade, shade, shade);
                    setRGB1(poly, 0, 0, 0);
                    setRGB2(poly, shade, shade, shade);
                    setRGB3(poly, 0, 0, 0);
                } else {
                    setRGB1(poly, shade, shade, shade);
                    setRGB0(poly, 0, 0, 0);
                    setRGB3(poly, shade, shade, shade);
                    setRGB2(poly, 0, 0, 0);
                }
                if (reverse == 0) {
                    edge = 0x20;
                    if (width >= 0xdf) {
                        edge = 0xfe - width;
                    }
                    setUVWH(poly, width, 0xe0, edge, 0x14);
                    setXYWH(poly, x + 0x80 + width + dx, y + 0x60 + dy, edge, height);
                } else {
                    edge = 0x20;
                    if (width < 0x20) {
                        edge = width;
                    }
                    setUVWH(poly, width - edge, 0xe0, edge, 0x14);
                    setXYWH(poly, x + (0x80 + width) - edge + dx, y + 0x60 + dy, edge, height);
                }
            }
            poly++;
        }
    }
    if (reverse != 0) {
        gfx_draw_or_append_gpu_primitive(&polys[1]);
        gfx_draw_or_append_gpu_primitive(&polys[0]);
        gfx_draw_or_append_gpu_primitive(&polys[3]);
        gfx_draw_or_append_gpu_primitive(&polys[2]);
    } else {
        gfx_draw_or_append_gpu_primitive(&polys[3]);
        gfx_draw_or_append_gpu_primitive(&polys[2]);
        gfx_draw_or_append_gpu_primitive(&polys[1]);
        gfx_draw_or_append_gpu_primitive(&polys[0]);
    }
}

/* Thread body that animates the map-title panel strip with double-buffered
 * primitives: widen it by the thread's step, hold it, then sweep it again
 * with `reverse` set. The hold phase reuses `width` as its frame counter. */
void attack_map_title_entrypoint(void) {
    s32 step = thread_current_parameter_3_get();
    s32 buffer = 0;
    s32 width;

    if (step == 0) {
        step = 1;
    }

    width = 8;
    do {
        buffer ^= 1;
        battle_map_title_build_strips(g_battle_map_title_sprites[buffer], width, 0x80, 0);
        width += step;
        thread_yield();
    } while (width < 0xf8);

    width = 0;
    do {
        buffer ^= 1;
        battle_map_title_build_strips(g_battle_map_title_sprites[buffer], 0xf8, 0x80, 0);
        width += step;
        thread_yield();
    } while (width < 0x6e);

    width = 8;
    do {
        buffer ^= 1;
        battle_map_title_build_strips(g_battle_map_title_sprites[buffer], width, 0x80, 1);
        if (width >= 0x81) {
            s_battle_map_title_closing_past_midpoint = 1;
        }
        width += step;
        thread_yield();
    } while (width < 0xf9);

    thread_current_exit();
}

/*
 * Upload the current map's title graphic and its palette.
 *
 * The titles are stored four to a 0x2800-byte file, 0xa00 bytes each, in
 * files five sectors apart from sector 0xdac, indexed by map id - 1. For map
 * 0x100 no file is read and the freshly allocated buffer is uploaded as it
 * is.
 */
void battle_map_title_load_graphic(void) {
    s32 group;
    u8* image = menu_alloc_memory(0x2800);
    s32 title = script_variable_get(EVENT_SCRIPT_VAR_CURRENT_MAP);
    s32 adjusted = title - 1;
    s32 group_input = adjusted;

    if (adjusted < 0) {
        group_input = title + 2;
    }
    group_input = group = group_input >> 2;
    group = adjusted - group * 4;

    if (adjusted != 0xff) {
        title = group_input * 5 + DISC_SECTOR_EVENT_MAPTITLE_BIN;
        g_thread_call_target = (void (*)(void))file_buffer_load_checked;
        thread_call_on_main_stack(title, 0x2800, image);
    }

    LoadImage(&g_battle_map_title_image_rect, (u32*)(image + group * 0xa00));
    LoadImage(&g_battle_map_title_palette_rect, g_battle_map_title_palette_data);
    menu_free_memory(image);
}
