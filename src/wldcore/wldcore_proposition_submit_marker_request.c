#include "fft/main_runtime.h"
#include "fft/wldcore.h"

/* Builds the world-map request block for the current entry at 0x800d0bb4 and
 * submits it, unless system flag 0x2000 suppresses it or no entry is active.
 *
 * The colour is the shade word at 0x8009eef4 replicated across the three
 * channels and tinted in place by 0x80069718. sequence is written after that
 * call because the target reloads 0x800d0bb4 there. */
void wldcore_proposition_submit_marker_request(s32 ot) {
    wldcore_anim_draw_request_t request;
    CVECTOR color;
    s32 index;
    s32 shade;

    if (g_main_system_flags & 0x2000) {
        return;
    }
    if (g_wldcore_proposition_selected_entry == 0) {
        return;
    }

    index = g_wldcore_proposition_selected_entry - 1;
    request.flags = 1;
    request.x = g_wldcore_map_dot_screen_x[index].value;
    request.y = g_wldcore_map_dot_screen_y[index].value;
    shade = g_wldcore_location_marker_shade;
    request.priority = 4;
    request.palette = 0;
    request.anim_counter = 0;
    request.frame_index = 0;
    color.r = shade;
    color.g = shade;
    color.b = shade;
    wldcore_gfx_copy_color_with_tint(&color, &color);
    request.color.r = color.r;
    request.color.g = color.g;
    request.color.b = color.b;
    request.sequence = g_wldcore_proposition_selected_entry + 0x36;
    wldcore_gfx_draw_animated_display_object((wldcore_anim_object_t*)&request, (void*)ot);
}
