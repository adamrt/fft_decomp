#include "fft/open.h"

void open_gfx_init_render_buffers(void) {
    s32 i;
    u8* color;

    g_open_gfx_primitive_count = 0;
    g_open_gfx_next_render_record_56 = 0;
    g_open_gfx_next_render_record_36 = 0;
    g_open_gfx_render_record_pointer_count = 0;

    for (i = 0; i < 16; i++) {
        g_open_gfx_render_records_36[i].flags = 1;
        g_open_gfx_render_records_36[i].ot_layer = 8;
        color = &g_open_gfx_render_records_36[i].r;
        color[0] = 0x80;
        color[1] = 0x80;
        color[2] = 0x80;
    }

    for (i = 0; i < 16; i++) {
        g_open_gfx_render_records_56[i].flags = 2;
        g_open_gfx_render_records_56[i].ot_layer = 8;
        color = &g_open_gfx_render_records_56[i].color.r;
        color[0] = 0x80;
        color[1] = 0x80;
        color[2] = 0x80;
    }
}
