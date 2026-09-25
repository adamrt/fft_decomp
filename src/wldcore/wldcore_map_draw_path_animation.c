#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Draws the slideshow pages shown so far as textured semi-transparent quads:
 * every page up to page_index is projected from its four outline points, and
 * the current page's far edge is interpolated by frame_timer / page_frames so
 * it unrolls over the page's display time. A negative page_index or one equal
 * to page_count stops the slideshow instead.
 *
 * The x and y columns are read through two pointers set at the top of each
 * page, which loop optimisation hoists into their own registers; page_index is
 * read into `page` first so it keeps its own base register. The projection
 * loop is a goto loop, so nothing in it is strength-reduced, inside a
 * `do ... while (0)` whose loop notes weight its references for register
 * allocation as the target's (the column pointers then outrank `ot`, which is
 * spilled). */
void wldcore_map_draw_path_animation(GsOT* ot) {
    SVECTOR verts[4];
    SVECTOR in;
    POLY_FT4 poly;
    s32 projected;
    s32 i;
    s32 j;
    s32 k;
    s32 frames;
    s32 length;
    s32 page;
    s16* xs;
    s16* ys;

    if (!(g_wldcore_map_path_animation.flags & 1)) {
        return;
    }
    SetPolyFT4(&poly);
    SetSemiTrans(&poly, 1);
    poly.u0 = 0x88;
    poly.v0 = 0xE0;
    poly.u1 = 0x97;
    poly.v1 = 0xE0;
    poly.u2 = 0x88;
    poly.v2 = 0xEF;
    poly.u3 = 0x97;
    poly.v3 = 0xEF;
    poly.r0 = g_wldcore_screen_brightness_rgb[0];
    poly.g0 = g_wldcore_screen_brightness_rgb[1];
    poly.b0 = g_wldcore_screen_brightness_rgb[2];
    poly.tpage = GetTPage(0, 0, 0x1C0, 0x100);
    poly.clut = GetClut(0x10, 0x1E8);
    if (g_wldcore_map_path_animation.page_index < 0
        || g_wldcore_map_path_animation.page_index == g_wldcore_map_path_animation.page_count) {
        g_wldcore_map_path_animation.flags &= ~1;
        return;
    }
    for (i = 0; i < g_wldcore_map_path_animation.page_index + 1; i++) {
        page = g_wldcore_map_path_animation.page_index;
        xs = g_wldcore_map_path_animation.coords;
        ys = g_wldcore_map_path_animation.coords + 1;
        if (i == page) {
            frames = (s16)g_wldcore_map_path_animation.frame_timer;
        } else {
            frames = g_wldcore_map_path_animation.page_frames[i];
        }
        do {
            j = 0;
            k = i * 4;
        loop:
            in.vx = xs[k];
            in.vy = ys[k];
            in.vz = 0;
            RotTransSV(&in, &verts[j], &projected);
            k += 2;
            j++;
            if (j < 4) {
                goto loop;
            }
        } while (0);
        length = g_wldcore_map_path_animation.page_frames[i];
        if (frames != length) {
            verts[2].vx = verts[0].vx + (verts[2].vx - verts[0].vx) * frames / length;
            verts[2].vy
                = verts[0].vy + (verts[2].vy - verts[0].vy) * frames / g_wldcore_map_path_animation.page_frames[i];
            verts[3].vx
                = verts[1].vx + (verts[3].vx - verts[1].vx) * frames / g_wldcore_map_path_animation.page_frames[i];
            verts[3].vy
                = verts[1].vy + (verts[3].vy - verts[1].vy) * frames / g_wldcore_map_path_animation.page_frames[i];
        }
        poly.x0 = verts[0].vx;
        poly.y0 = verts[0].vy;
        poly.x1 = verts[1].vx;
        poly.y1 = verts[1].vy;
        poly.x2 = verts[2].vx;
        poly.y2 = verts[2].vy;
        poly.x3 = verts[3].vx;
        poly.y3 = verts[3].vy;
        /* This caller sets only a0-a2, hence the three-argument cast. */
        ((void (*)(POLY_FT4*, s32, s32))world_gs_sortpoly)(&poly, (s32)ot, 1);
    }
}
