#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/event.h"
#include "fft/main_heap.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* The callers pass unrelated pointers, so the parameters are void* and the
 * typed view is a cast at each use. A cast of a pointer parameter is the
 * parameter's own pseudo and emits nothing, whereas assigning it to a typed
 * local would add copies that cost the match. */
#define PAIR ((battle_gfx_scaled_draw_area_pair_t*)packets)

/*
 * Build a double-buffered draw-area pair whose rectangle is cropped to a
 * percentage of `geometry` about its centre. Sources at x < 0x80 are moved
 * to the right-hand page (x + 0x80, y + 0x78); others are moved left.
 */
void battle_menu_build_zoom_draw_area_pair(void* packets, void* geometry, s32 step, s32 flag) {
    s32 percent;
    u16 width;
    u16 height;
    s32 offset;

    if (g_battle_event_speed == 2) {
        step *= 2;
    }
    if (step >= 12) {
        step = 11;
    }
    percent = g_battle_menu_zoom_percentages[step];
    battle_copy_bytes(&PAIR->rects[0], (const RECT*)geometry, 8);
    if (PAIR->rects[0].x < 0x80) {
        PAIR->rects[0].x += 0x80;
        PAIR->rects[0].y += 0x78;
    } else {
        PAIR->rects[0].x -= 0x80;
    }
    width = PAIR->rects[0].w;
    height = PAIR->rects[0].h;
    offset = ((s16)width * percent) / 200;
    PAIR->rects[0].x = (s16)width / 2 + PAIR->rects[0].x - offset;
    offset = ((s16)height * percent) / 200;
    PAIR->rects[0].y = (s16)height / 2 + PAIR->rects[0].y - offset;
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
