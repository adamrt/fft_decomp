/* LIBGPU 80022f2c-80022fd0. */
#include "psx/libgpu.h"
void FntLoad(int x, int y) {
    g_psyq_gpu_font_clut = LoadClut2((u32*)g_psyq_gpu_font_asset.palette, x, y + 128);
    g_psyq_gpu_font_tpage = LoadTPage(g_psyq_gpu_font_asset.pixels[0], 0, 0, x, y, 128, 32);
    g_psyq_gpu_font_window_count = 0;
    memset(Font, 0, sizeof(Font));
}
