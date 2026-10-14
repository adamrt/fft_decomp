#include "fft/wldcore.h"

/* Builds the world-map request block for the current entry at 0x800d0bb4 and
 * submits it, unless system flag 0x2000 suppresses it or no entry is active.
 *
 * The color is the 32-bit shade value at 0x8009eef4 replicated across the three
 * channels and tinted in place by 0x80069718. sequence is written after that
 * call because the target reloads 0x800d0bb4 there. */
void wldcore_proposition_submit_marker_request(GsOT* ot) {
    world_animated_render_record_t request;
    CVECTOR color;
    s32 index;
    s32 shade;

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER) {
        return;
    }
    if (g_wldcore_proposition_selected_entry == 0) {
        return;
    }

    index = g_wldcore_proposition_selected_entry - 1;
    request.flags = WORLD_RENDER_RECORD_ANIMATED;
    request.position.x = g_wldcore_map_dots[index].render.position.x;
    request.position.y = g_wldcore_map_dots[index].render.position.y;
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
    request.sequence = g_wldcore_proposition_selected_entry + (WLDCORE_ANIM_SEQUENCE_LOCATION_NAME_FIRST - 1);
    wldcore_gfx_draw_animated_display_object(&request, ot);
}
