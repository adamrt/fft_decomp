#include "fft/open.h"

/* Empty both render record pools and the pointer list. Every record gets its
 * layout flag, ordering-table entry 8 and the neutral color 0x80. */
void window_init_record_pools_variant(void) {
    s32 i;
    CVECTOR* color;

    g_open_gfx_primitive_count = 0;
    g_open_gfx_next_render_record_56 = 0;
    g_open_gfx_next_render_record_36 = 0;
    g_open_gfx_render_record_pointer_count = 0;

    for (i = 0; i < OPEN_GFX_RENDER_RECORD_CAPACITY; i++) {
        g_open_gfx_render_records_36[i].flags = WORLD_RENDER_RECORD_ANIMATED;
        g_open_gfx_render_records_36[i].priority = 8;
        color = &g_open_gfx_render_records_36[i].color;
        color->b = color->g = color->r = 0x80;
    }

    for (i = 0; i < OPEN_GFX_RENDER_RECORD_CAPACITY; i++) {
        g_open_gfx_render_records_56[i].flags = WORLD_RENDER_RECORD_IMAGE;
        g_open_gfx_render_records_56[i].ot_layer = 8;
        color = &g_open_gfx_render_records_56[i].color;
        color->b = color->g = color->r = 0x80;
    }
}
