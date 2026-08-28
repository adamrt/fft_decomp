#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/gs.h"
#include "psx/types.h"

/* GsSortClear: register a full-screen clear tile for the active buffer. */
void world_gs_sortclear(u8 r, u8 g, u8 b, GsOT* otp) {
    s32 i;
    s16 x;
    s16 y;
    s16 h;

    g_world_gs_clear_tile[g_world_gs_active_buffer].r0 = r;
    g_world_gs_clear_tile[g_world_gs_active_buffer].g0 = g;
    g_world_gs_clear_tile[g_world_gs_active_buffer].b0 = b;
    i = g_world_gs_active_buffer;
    x = g_world_gs_buffer_x[i];
    h = g_world_gs_screen_height;
    g_world_gs_clear_tile[i].x0 = x;
    y = g_world_gs_buffer_y[i];
    g_world_gs_clear_tile[i].h = h;
    g_world_gs_clear_tile[i].y0 = y;
    if (g_world_gs_dispenv.isrgb24 != 0) {
        g_world_gs_clear_tile[i].w = g_world_gs_screen_width * 3 / 2;
    } else {
        g_world_gs_clear_tile[i].w = g_world_gs_screen_width;
    }
    AddPrim(otp->tag, &g_world_gs_clear_tile[g_world_gs_active_buffer]);
}
