/* An uncalled builder of a double-buffered draw area pair cropped about its center. */

#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

extern u16 g_screen_draw_area_scale_percent[];

/*
 * WORLD's copy of menu_build_scaled_draw_area_packets, beside its copy of the OPTION.OUT list
 * code; nothing calls it.
 * Build a double-buffered draw-area pair whose rectangle is `source` moved
 * left 0x80, shifted by the panel's position and
 * cropped to a percentage of its size about its center. The percentage comes
 * from a 12-step zoom table indexed by `step` (doubled at event speed 2, the
 * last step held). Nonzero `upper_half` targets the upper frame buffer; zero moves both
 * rectangles to the lower one at y 240.
 *
 * One `crop` variable carries both halves of the crop: giving each axis its
 * own local makes the y subtraction a second allocno, which schedules three
 * instructions early and rotates every temporary.
 */
void option_build_scaled_draw_area_packets(
    scaled_draw_area_pair_t* pair, const RECT* source, s32 step, s32 upper_half, const point32_t* position) {
    s32 percent;
    s16 w;
    s16 h;
    s32 crop;

    if (g_script_event_speed == 2) {
        step *= 2;
    }
    if (step >= 12) {
        step = 11;
    }
    percent = g_screen_draw_area_scale_percent[step];
    mem_copy_bytes_nonempty(&pair->rects[0], source, sizeof(pair->rects[0]));
    w = pair->rects[0].w;
    h = pair->rects[0].h;
    pair->rects[0].x -= 0x80;
    pair->rects[0].x += position->x;
    pair->rects[0].y += position->y;
    crop = (w * percent) / 200;
    pair->rects[0].x = w / 2 + pair->rects[0].x - crop;
    crop = (h * percent) / 200;
    pair->rects[0].y = h / 2 + pair->rects[0].y - crop;
    pair->rects[0].w = (pair->rects[0].w * percent) / 100;
    pair->rects[0].h = (pair->rects[0].h * percent) / 100;
    if (upper_half != 0) {
        pair->rects[1].y = 0;
    } else {
        pair->rects[1].y = 0xF0;
        pair->rects[0].y += 0xF0;
    }
    SetDrawArea(&pair->areas[0], &pair->rects[0]);
    SetDrawArea(&pair->areas[1], &pair->rects[1]);
}
