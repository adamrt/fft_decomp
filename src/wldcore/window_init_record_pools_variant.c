#include "fft/wldcore.h"
#include "psx/types.h"

/* Reset both world-map window pools and their allocation counters. */
void window_init_record_pools_variant(void) {
    /* Member pointers preserve the separate color and CLUT address walks. */
    CVECTOR* color;
    point32_t* clut;
    s32 i;

    g_wldcore_window_render_record_count = 0;
    g_wldcore_window_record_count = 0;
    g_wldcore_window_render_object_count = 0;
    g_wldcore_window_aux_render_object_count = 0;
    for (i = 0; i < WLDCORE_WINDOW_RECORD_CAPACITY; i++) {
        g_wldcore_window_records[i].flags = WORLD_RENDER_RECORD_ANIMATED;
        g_wldcore_window_records[i].priority = 1;
        color = &g_wldcore_window_records[i].color;
        color->b = color->g = color->r = 0x80;
    }

    for (i = 0; i < WLDCORE_WINDOW_RECORD_CAPACITY; i++) {
        g_wldcore_window_render_records[i].flags = WORLD_RENDER_RECORD_IMAGE;
        g_wldcore_window_render_records[i].priority = 1;
        clut = &g_wldcore_window_render_records[i].clut;
        clut->x = 0x3c0;
        clut->y = 0x1f0;
        color = &g_wldcore_window_render_records[i].color;
        color->b = color->g = color->r = 0x80;
    }
}
