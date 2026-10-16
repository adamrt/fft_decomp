/* The double-buffered draw area pair of a window's open and close zoom, kept at the left edge. */

#include "fft/event_require.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_panel.h"
#include "fft/script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Build the double-buffered draw-area pair of a window's open/close zoom.
 *
 * The first rectangle is `source` moved left one page (0x80), shifted by the
 * thread's x and y, and cropped about its center to
 * g_menu_draw_area_scale_percent[scale_index] percent of its size;
 * scale_index is doubled at event speed 2 and capped at 11. The second
 * rectangle is the frame buffer's own area. With `lower_half` zero both are
 * placed in the buffer at y 0xF0. REQUIRE then forces both
 * rectangles to x 0, discarding the horizontal crop position.
 */
void require_gfx_build_scaled_draw_area_packets(scaled_draw_area_pair_t* packet, const RECT* source, s32 scale_index,
    s32 lower_half, const menu_status_thread_t* thread) {
    s32 percent;
    s32 w;
    s32 h;
    s32 scaled;

    if (g_script_event_speed == 2) {
        scale_index *= 2;
    }
    if (scale_index >= 12) {
        scale_index = 11;
    }
    percent = g_menu_draw_area_scale_percent[scale_index];
    mem_copy_bytes_nonempty(&packet->rects[0], source, 8);
    w = packet->rects[0].w;
    h = packet->rects[0].h;
    packet->rects[0].x -= 0x80;
    packet->rects[0].x += thread->x;
    packet->rects[0].y += thread->y;
    scaled = (w * percent) / 200;
    packet->rects[0].x = w / 2 + packet->rects[0].x - scaled;
    scaled = (h * percent) / 200;
    packet->rects[0].y = h / 2 + packet->rects[0].y - scaled;
    packet->rects[0].w = (packet->rects[0].w * percent) / 100;
    packet->rects[0].h = (packet->rects[0].h * percent) / 100;
    if (lower_half != 0) {
        packet->rects[1].y = 0;
    } else {
        packet->rects[1].y = 0xF0;
        packet->rects[0].y += 0xF0;
    }
    packet->rects[0].x = 0;
    packet->rects[1].x = 0;
    SetDrawArea(&packet->areas[0], &packet->rects[0]);
    SetDrawArea(&packet->areas[1], &packet->rects[1]);
}
