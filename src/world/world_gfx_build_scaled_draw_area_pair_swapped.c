#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* The five callers pass unrelated pointers -- a primitive's cursor_mode field,
 * a record's texture window -- so the parameters are void* and the typed view
 * is a cast at each use. A cast of a pointer parameter is the parameter's own
 * pseudo and emits nothing, whereas assigning it to a typed local would add the
 * copies that cost the match in world_gfx_build_scaled_draw_area_pair_at_offset. */
#define PAIR ((world_gfx_scaled_draw_area_pair_t*)packets)

/*
 * Build a double-buffered draw-area pair whose rectangle is cropped to a
 * percentage of `geometry` about its centre. Sources at x < 0x80 are moved
 * to the right-hand page (x + 0x80, y + 0x78); others are moved left.
 */
void world_gfx_build_scaled_draw_area_pair_swapped(void* packets, void* geometry, s32 step, s32 flag) {
    s32 percent;
    u16 w;
    u16 h;
    s32 half_extent;

    if (g_world_event_speed == 2) {
        step *= 2;
    }
    if (step >= 12) {
        step = 11;
    }
    percent = g_world_menu_cursor_zoom_percentages[step];
    world_script_copy_bytes(&PAIR->rects[0], (const RECT*)geometry, 8);
    if (PAIR->rects[0].x < 0x80) {
        PAIR->rects[0].x += 0x80;
        PAIR->rects[0].y += 0x78;
    } else {
        PAIR->rects[0].x -= 0x80;
    }
    w = PAIR->rects[0].w;
    h = PAIR->rects[0].h;
    half_extent = ((s16)w * percent) / 200;
    PAIR->rects[0].x = (s16)w / 2 + PAIR->rects[0].x - half_extent;
    half_extent = ((s16)h * percent) / 200;
    PAIR->rects[0].y = (s16)h / 2 + PAIR->rects[0].y - half_extent;
    PAIR->rects[0].w = (PAIR->rects[0].w * percent) / 100;
    PAIR->rects[0].h = (PAIR->rects[0].h * percent) / 100;
    if (flag != 0) {
        PAIR->rects[1].y = 0;
    } else {
        PAIR->rects[1].y = 0xF0;
        PAIR->rects[0].y += 0xF0;
    }
    SetDrawArea(&PAIR->areas[0], &PAIR->rects[0]);
    SetDrawArea(&PAIR->areas[1], &PAIR->rects[1]);
}
