#include "fft/wldcore.h"
#include "psx/types.h"

typedef struct wldcore_render_geometry_init {
    s32 clut_x;
    s32 clut_y;
    u8 rest[0x2c];
} wldcore_render_geometry_init_t;

typedef char wldcore_render_geometry_init_size_must_be_0x34[sizeof(wldcore_render_geometry_init_t) == 0x34 ? 1 : -1];

/* Reset both world-map window pools and their allocation counters. */
void wldcore_window_init_record_pools(void) {
    wldcore_window_entry_36_rgb_t* window_color;
    wldcore_render_geometry_init_t* render_geometry;
    wldcore_window_entry_52_rgb_t* render_color;
    s32 i;

    window_color = (wldcore_window_entry_36_rgb_t*)&g_wldcore_window_records->color;
    g_wldcore_window_render_record_count = 0;
    g_wldcore_window_record_count = 0;
    g_wldcore_window_render_object_count = 0;
    g_wldcore_window_aux_render_object_count = 0;
    for (i = 0; i < 0x20; i++) {
        g_wldcore_window_records[i].flags = 1;
        g_wldcore_window_records[i].priority = 1;
        window_color[i].red = 0x80;
        window_color[i].green = 0x80;
        window_color[i].blue = 0x80;
    }

    render_geometry = (wldcore_render_geometry_init_t*)&g_wldcore_window_render_records->clut_x;
    render_color = (wldcore_window_entry_52_rgb_t*)&g_wldcore_window_render_records->red;
    for (i = 0; i < 0x20; i++) {
        g_wldcore_window_render_records[i].flags = 2;
        g_wldcore_window_render_records[i].priority = 1;
        render_geometry[i].clut_x = 0x3c0;
        render_geometry[i].clut_y = 0x1f0;
        render_color[i].red = 0x80;
        render_color[i].green = 0x80;
        render_color[i].blue = 0x80;
    }
}
