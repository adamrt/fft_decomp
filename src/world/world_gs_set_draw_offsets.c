#include "fft/world.h"
#include "psx/types.h"

void world_gs_set_draw_offsets(s32 x0, s32 y0, s32 x1, s32 y1) {
    g_world_gs_buffer_x[0] = x0;
    g_world_gs_buffer_x[1] = x1;
    g_world_gs_buffer_y[0] = y0;
    g_world_gs_buffer_y[1] = y1;
    if (g_world_gs_offset_draw_active != 0) {
        g_world_gs_buffer_offset_x[0] = 0;
        g_world_gs_buffer_offset_x[1] = 0;
        g_world_gs_buffer_offset_y[0] = 0;
        g_world_gs_buffer_offset_y[1] = 0;
    } else {
        g_world_gs_buffer_offset_x[0] = x0;
        g_world_gs_buffer_offset_x[1] = x1;
        g_world_gs_buffer_offset_y[0] = y0;
        g_world_gs_buffer_offset_y[1] = y1;
    }
    world_gs_setdrawbuffclip();
    world_gs_setdrawbuffoffset();
}
