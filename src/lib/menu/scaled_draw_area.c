/* Scaled window draw-area packet construction and initialization. */

#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Build the double-buffered draw-area pair of a window's open/close zoom.
 *
 * The first rectangle is `source` moved left one page (0x80), shifted by the
 * thread's x and y, and cropped about its center to
 * g_menu_draw_area_scale_percent[scale_index] percent of its size;
 * scale_index is doubled at event speed 2 and capped at 11. The second
 * rectangle is the frame buffer's own area. Nonzero `upper_half` places both
 * in the buffer at y 0; zero selects the buffer at y 240.
 */
void menu_build_scaled_draw_area_packets(
    scaled_draw_area_pair_t* packet, const RECT* source, s32 scale_index, s32 upper_half, const point32_t* position) {
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
    mem_copy_bytes_nonempty(&packet->rects[0], source, sizeof(RECT));
    w = packet->rects[0].w;
    h = packet->rects[0].h;
    packet->rects[0].x -= 0x80;
    packet->rects[0].x += position->x;
    packet->rects[0].y += position->y;
    scaled = (w * percent) / 200;
    packet->rects[0].x = w / 2 + packet->rects[0].x - scaled;
    scaled = (h * percent) / 200;
    packet->rects[0].y = h / 2 + packet->rects[0].y - scaled;
    packet->rects[0].w = (packet->rects[0].w * percent) / 100;
    packet->rects[0].h = (packet->rects[0].h * percent) / 100;
    if (upper_half != 0) {
        packet->rects[1].y = 0;
    } else {
        packet->rects[1].y = 0xF0;
        packet->rects[0].y += 0xF0;
    }
#ifdef FFT_EVENT_REQUIRE
    /* REQUIRE anchors both rectangles at the left edge; WORLD and the other overlays keep the horizontal crop. */
    packet->rects[0].x = 0;
    packet->rects[1].x = 0;
#endif
    SetDrawArea(&packet->areas[0], &packet->rects[0]);
    SetDrawArea(&packet->areas[1], &packet->rects[1]);
}

/* WORLD compiles its three initializer copies separately. */
#ifndef FFT_WORLD
/* Clear the first draw area's width and install the default second draw area. */
void menu_init_scaled_draw_area_packets(scaled_draw_area_pair_t* data) {
    data->rects[0].w = 0;
    data->rects[1].h = 0;
    mem_copy_bytes_nonempty(&data->rects[1], &g_menu_draw_area_template, sizeof(RECT));
}
#endif
