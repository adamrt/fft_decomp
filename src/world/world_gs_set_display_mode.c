#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Reapply the libgs display/draw mode without resetting the GPU: unlike
 * world_gs_initgraph it only restages the environments and refreshes the
 * screen state. */
void world_gs_set_display_mode(u16 x, u16 y, u16 intmode, u16 dither, u16 vram) {
    g_world_gs_dispenv.disp.w = x;
    g_world_gs_dispenv.disp.h = y;
    g_world_gs_drawenv.tpage = 0;
    g_world_gs_drawenv.dtd = dither;
    g_world_gs_drawenv.dfe = 0;
    g_world_gs_drawenv.isbg = 0;
    g_world_gs_dispenv.isinter = intmode & 1;
    g_world_gs_offset_draw_active = intmode & 4;
    g_world_gs_dispenv.isrgb24 = vram;
    world_gs_reset_screen_state(x, y);
}
