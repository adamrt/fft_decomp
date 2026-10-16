/* An uncalled builder of a double-buffered draw area pair cropped about its center. */

#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/script.h"
#include "fft/world_menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * The menu-screen library's copy of world_option_gfx_build_scaled_draw_area_packets; nothing calls it.
 * Build a double-buffered draw-area pair whose rectangle is `source` moved
 * left 0x80, shifted by `offset` (x by offset->x, y by offset->w) and
 * cropped to a percentage of its size about its center. The percentage comes
 * from a 12-step zoom table indexed by `step` (doubled at event speed 2, the
 * last step held). `flag` set targets the upper frame buffer; clear, both
 * rectangles move to the lower one at y 240.
 *
 * One `crop` variable carries both halves of the crop: giving each axis its
 * own local makes the y subtraction a second allocno, which schedules three
 * instructions early and rotates every temporary.
 */
void world_screen_gfx_build_scaled_draw_area_packets(
    scaled_draw_area_pair_t* pair, const RECT* source, s32 step, s32 flag, const RECT* offset) {
    s32 percent;
    s32 w;
    s32 h;
    s32 crop;

    if (g_script_event_speed == 2) {
        step *= 2;
    }
    if (step >= 12) {
        step = 11;
    }
    percent = g_world_menu_zoom_percentages[step];
    mem_copy_bytes_nonempty(&pair->rects[0], source, sizeof(pair->rects[0]));
    w = pair->rects[0].w;
    h = pair->rects[0].h;
    pair->rects[0].x -= 0x80;
    pair->rects[0].x += offset->x;
    pair->rects[0].y += offset->w;
    crop = (w * percent) / 200;
    pair->rects[0].x = w / 2 + pair->rects[0].x - crop;
    crop = (h * percent) / 200;
    pair->rects[0].y = h / 2 + pair->rects[0].y - crop;
    pair->rects[0].w = (pair->rects[0].w * percent) / 100;
    pair->rects[0].h = (pair->rects[0].h * percent) / 100;
    if (flag != 0) {
        pair->rects[1].y = 0;
    } else {
        pair->rects[1].y = 0xF0;
        pair->rects[0].y += 0xF0;
    }
    SetDrawArea(&pair->areas[0], &pair->rects[0]);
    SetDrawArea(&pair->areas[1], &pair->rects[1]);
}
