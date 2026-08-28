#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

typedef union wldcore_map_dot_source_word {
    volatile s32 w;
    volatile u8 b[4];
} wldcore_map_dot_source_word_t;

typedef struct wldcore_map_dot_source {
    wldcore_map_dot_source_word_t a;
    volatile s32 y;
} wldcore_map_dot_source_t;

extern wldcore_map_dot_source_t* g_wldcore_map_dot_sources;

/* Flags-only view of g_wldcore_map_dots (0x800d3ca8). The hidden-flag update is a
 * volatile access in the target: reorg does not pull the loop-tail `dot++`
 * into the preceding branch delay slot across it, which the target leaves as
 * a nop. */
typedef struct wldcore_map_dot_hidden_view {
    u8 unknown_00[0x10];
    volatile s32 flags;
    u8 unknown_14[0x20];
} wldcore_map_dot_hidden_view_t;

extern wldcore_map_dot_hidden_view_t g_wldcore_map_dots_view[];

void wldcore_map_color_and_draw_dots(void) {
    wldcore_map_dot_source_t* src;
    wldcore_map_dot_t* dots;
    wldcore_map_dot_t* dot;
    u8* flags_base;
    u8* color;
    s32 y;
    s32 is_castle;
    s32 queue_index;
    s32 i;

    dots = g_wldcore_map_dots;
    /* Queue entries are built as flags_base + i * stride; &dots[i].flags does not match. */
    flags_base = (u8*)&dots->flags;
    dot = dots;
    src = g_wldcore_map_dot_sources;
    g_wldcore_map_dot_pulse_direction = 0;
    g_wldcore_map_dot_pulse_phase = 0;
    g_wldcore_map_dot_count = 0;
    for (i = 0; i < 0x2b; i++) {
        g_wldcore_map_dots[i].kind = src[i].a.b[2];
        g_wldcore_map_dots[i].sub_kind = src[i].a.b[3];
        g_wldcore_map_dots[i].position.vx = src[i].a.w;
        y = src[i].y;
        g_wldcore_map_dots[i].flags = 1;
        g_wldcore_map_dots[i].priority = 0xf;
        color = g_wldcore_map_dots[i].rgb;
        g_wldcore_map_dots[i].position.vz = 0;
        g_wldcore_map_dots[i].position.vy = y;
        color[0] = 0x80;
        color[1] = 0x80;
        color[2] = 0x80;
        is_castle = g_wldcore_map_dots[i].kind == 2;
        g_wldcore_map_dots[i].sprite_id = is_castle + 0x6a;
        if (world_script_get_variable(i + 0x267) != 0) {
            if (i < 0x18) {
                g_wldcore_map_dots[i].palette = 0xc;
            } else {
                g_wldcore_map_dots[i].palette = 0x10;
            }
        } else {
            g_wldcore_map_dots[i].palette = 0;
        }
        dot->anim_counter = 0;
        g_wldcore_map_dots[i].frame_index = 0;
        if (world_script_get_variable(i + 0x200) == 0) {
            g_wldcore_map_dots_view[i].flags |= 0x10;
        }
        dot++;
        queue_index = g_wldcore_window_render_object_count;
        g_wldcore_window_render_object_queue[queue_index] = (u32*)(i * sizeof(wldcore_map_dot_t) + (s32)flags_base);
        g_wldcore_window_render_object_count = queue_index + 1;
    }
    g_wldcore_map_dot_count = 0x2b;
    wldcore_map_project_and_cull_dots();
}
