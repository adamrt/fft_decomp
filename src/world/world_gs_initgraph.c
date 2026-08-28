#include "fft/world.h"
#include "psx/etc.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* libgs GsInitGraph counterpart. */
void world_gs_initgraph(u16 x, u16 y, u16 intmode, u16 dither, u16 vram) {
    ResetGraph(((intmode >> 4) & 3) == 3 ? 3 : 0);
    g_world_gs_drawenv.ofs[0] = g_world_gs_drawenv.ofs[1] = 0;
    g_world_gs_drawenv.tw.w = g_world_gs_drawenv.tw.h = 0;
    g_world_gs_drawenv.tw.x = g_world_gs_drawenv.tw.y = 0;
    g_world_gs_drawenv.tpage = 0;
    g_world_gs_drawenv.dtd = dither;
    g_world_gs_drawenv.dfe = 0;
    g_world_gs_drawenv.isbg = 0;
    PutDrawEnv(&g_world_gs_drawenv);
    g_world_gs_dispenv.disp.x = 0;
    g_world_gs_dispenv.disp.y = 0;
    g_world_gs_dispenv.disp.w = x;
    g_world_gs_dispenv.disp.h = y;
    g_world_gs_dispenv.screen.x = 0;
    g_world_gs_dispenv.screen.y = 0;
    g_world_gs_dispenv.screen.w = 0;
    g_world_gs_dispenv.screen.h = 0;
    if (GetVideoMode() == 1) {
        g_world_gs_dispenv.screen.y = 0x18;
        g_world_gs_dispenv.pad0 = 1;
    }
    g_world_gs_dispenv.isinter = intmode & 1;
    g_world_gs_offset_draw_active = intmode & 4;
    g_world_gs_dispenv.isrgb24 = vram;
    PutDispEnv(&g_world_gs_dispenv);
}
