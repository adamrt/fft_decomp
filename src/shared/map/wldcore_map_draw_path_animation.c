#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draws the slideshow pages shown so far as textured semi-transparent quads:
 * every page up to page_index is projected from its four outline points, and
 * the current page's far edge is interpolated by frame_timer / page_frames so
 * it unrolls over the page's display time. A negative page_index or one equal
 * to page_count stops the slideshow instead. */
void wldcore_map_draw_path_animation(GsOT* ot) {
    SVECTOR verts[4];
    SVECTOR in;
    POLY_FT4 poly;
    s32 projected;
    s32 i;
    s32 j;
    s32 frames;

    if (!(g_wldcore_map_path_animation.flags & WLDCORE_SLIDESHOW_RUNNING)) {
        return;
    }
    SetPolyFT4(&poly);
    SetSemiTrans(&poly, 1);
    setUV4(&poly, 0x88, 0xE0, 0x97, 0xE0, 0x88, 0xEF, 0x97, 0xEF);
    setRGB0(
        &poly, g_wldcore_screen_brightness_rgb.r, g_wldcore_screen_brightness_rgb.g, g_wldcore_screen_brightness_rgb.b);
    poly.tpage = GetTPage(0, 0, 0x1C0, 0x100);
    poly.clut = GetClut(0x10, 0x1E8);
    if (g_wldcore_map_path_animation.page_index < 0
        || g_wldcore_map_path_animation.page_index == g_wldcore_map_path_animation.page_count) {
        g_wldcore_map_path_animation.flags &= ~WLDCORE_SLIDESHOW_RUNNING;
        return;
    }
    for (i = 0; i < g_wldcore_map_path_animation.page_index + 1; i++) {
        if (i == g_wldcore_map_path_animation.page_index) {
            frames = g_wldcore_map_path_animation.frame_timer;
        } else {
            frames = g_wldcore_map_path_animation.page_frames[i];
        }
        for (j = 0; j < 4; j++) {
            in.vx = g_wldcore_map_path_animation.coords[i * 4 + j * 2];
            in.vy = g_wldcore_map_path_animation.coords[i * 4 + j * 2 + 1];
            in.vz = 0;
            RotTransSV(&in, &verts[j], &projected);
        }
        if (frames != g_wldcore_map_path_animation.page_frames[i]) {
            verts[2].vx
                = verts[0].vx + (verts[2].vx - verts[0].vx) * frames / g_wldcore_map_path_animation.page_frames[i];
            verts[2].vy
                = verts[0].vy + (verts[2].vy - verts[0].vy) * frames / g_wldcore_map_path_animation.page_frames[i];
            verts[3].vx
                = verts[1].vx + (verts[3].vx - verts[1].vx) * frames / g_wldcore_map_path_animation.page_frames[i];
            verts[3].vy
                = verts[1].vy + (verts[3].vy - verts[1].vy) * frames / g_wldcore_map_path_animation.page_frames[i];
        }
        setXY4(&poly, verts[0].vx, verts[0].vy, verts[1].vx, verts[1].vy, verts[2].vx, verts[2].vy, verts[3].vx,
            verts[3].vy);
        GsSortPoly(&poly, ot, 1);
    }
}
