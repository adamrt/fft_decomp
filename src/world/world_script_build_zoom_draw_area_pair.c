#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Twelve crop percentages, indexed by step. */

/*
 * Build a double-buffered draw-area pair whose rectangle is `source` moved
 * left one page, shifted by `offset` (x by offset->x, y by offset->w) and
 * cropped to a percentage of its size about its centre.
 *
 * One `crop` variable carries both halves of the crop: giving each axis its
 * own local makes the y subtraction a second allocno, which schedules three
 * instructions early and rotates every temporary.
 */
void world_script_build_zoom_draw_area_pair(
    world_gfx_scaled_draw_area_pair_t* pair, const RECT* source, s32 step, s32 flag, const RECT* offset) {
    s32 percent;
    u16 w;
    u16 h;
    s32 crop;

    if (g_world_event_speed == 2) {
        step *= 2;
    }
    if (step >= 12) {
        step = 11;
    }
    percent = g_world_gfx_draw_area_scale_percent[step];
    world_script_copy_bytes(&pair->rects[0], source, 8);
    w = pair->rects[0].w;
    h = pair->rects[0].h;
    pair->rects[0].x -= 0x80;
    pair->rects[0].x += offset->x;
    pair->rects[0].y += offset->w;
    crop = ((s16)w * percent) / 200;
    pair->rects[0].x = (s16)w / 2 + pair->rects[0].x - crop;
    crop = ((s16)h * percent) / 200;
    pair->rects[0].y = (s16)h / 2 + pair->rects[0].y - crop;
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
