#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/*
 * Builds the 43 world-map location dots and queues them for drawing.
 *
 * Each dot takes its kind, sub-kind and map position from
 * g_wldcore_map_dot_sources; kind 2 uses the second dot sequence. A location
 * whose color script variable is set gets palette 0xc (locations 0..23) or
 * 0x10 (the rest), any other the default palette, and one whose visibility
 * variable is 0 is hidden. Resets the pulse state and projects the dots.
 */
void wldcore_map_color_and_draw_dots(void) {
    wldcore_map_dot_source_t* src;
    wldcore_map_dot_t* dots;
    wldcore_map_dot_t* dot;
    s32* flags_base;
    CVECTOR* color;
    s32 y;
    s32 is_castle;
    s32 queue_index;
    s32 i;

    dots = g_wldcore_map_dots;
    /* Queue entries are built from the flags column base; &dots[i].render.flags does not match. */
    flags_base = &dots->render.flags;
    dot = dots;
    src = g_wldcore_map_dot_sources;
    g_wldcore_map_dot_pulse_direction = 0;
    g_wldcore_map_dot_pulse_phase = 0;
    g_wldcore_map_dot_count = 0;
    for (i = 0; i < WORLD_LOCATION_COUNT; i++) {
        g_wldcore_map_dots[i].kind = (u8)(src[i].position_and_kind >> 16);
        g_wldcore_map_dots[i].sub_kind = (u8)(src[i].position_and_kind >> 24);
        /* The x coordinate occupies the packed 32-bit value's low 16 bits. */
        g_wldcore_map_dots[i].position.vx = src[i].position_and_kind;
        y = src[i].y;
        g_wldcore_map_dots[i].render.flags = WORLD_RENDER_RECORD_ANIMATED;
        g_wldcore_map_dots[i].render.priority = 0xf;
        /* Direct nested access biases the base to render; retail addresses the color column. */
        color = (CVECTOR*)((u8*)&g_wldcore_map_dots[i] + offsetof(wldcore_map_dot_t, render.color));
        g_wldcore_map_dots[i].position.vz = 0;
        g_wldcore_map_dots[i].position.vy = y;
        color->r = 0x80;
        color->g = 0x80;
        color->b = 0x80;
        is_castle = g_wldcore_map_dots[i].kind == 2;
        g_wldcore_map_dots[i].render.sequence = is_castle + WLDCORE_ANIM_SEQUENCE_MAP_DOT;
        if (script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_COLOR_FIRST) != 0) {
            if (i < WORLD_LOCATION_FIELD_FIRST) {
                g_wldcore_map_dots[i].render.palette = 0xc;
            } else {
                g_wldcore_map_dots[i].render.palette = 0x10;
            }
        } else {
            g_wldcore_map_dots[i].render.palette = 0;
        }
        dot->render.anim_counter = 0;
        g_wldcore_map_dots[i].render.frame_index = 0;
        if (script_get_variable(i + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST) == 0) {
            g_wldcore_map_dots[i].render.flags |= WORLD_RENDER_RECORD_HIDDEN;
        }
        dot++;
        queue_index = g_wldcore_window_render_object_count;
        g_wldcore_window_render_object_queue[queue_index] = (void*)(i * sizeof(wldcore_map_dot_t) + (s32)flags_base);
        g_wldcore_window_render_object_count = queue_index + 1;
    }
    g_wldcore_map_dot_count = WORLD_LOCATION_COUNT;
    wldcore_map_project_and_cull_dots();
}
