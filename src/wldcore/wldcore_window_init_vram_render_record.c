#include "fft/wldcore.h"

/* Prepare a render record for an image that is already in VRAM.
 *
 * Caller flags replace bits 0x18; texture coordinates use doubled 4bpp units.
 */
void wldcore_window_init_vram_render_record(
    s32 index, wldcore_xy16_t position, wldcore_xy16_t dimensions, wldcore_point32_t clut, s32 flags) {
    wldcore_window_render_bounds16_t* bounds;
    u8* rgb;

    g_wldcore_window_render_records[index].flags = (g_wldcore_window_render_records[index].flags & ~0x18) | flags;
    *(wldcore_point32_t*)&g_wldcore_window_render_records[index].clut_x = clut;
    g_wldcore_window_render_records[index].tpage = GetTPage(0, 0, (s16)position.x, (s16)position.y) & 0xFFFF;
    bounds = (wldcore_window_render_bounds16_t*)&g_wldcore_window_render_records[index].x;
    bounds->position.x = (position.x & 0x3F) * 2;
    bounds->position.y = (u8)position.y;
    bounds->dimensions.x = (s16)dimensions.x * 2;
    bounds->dimensions.y = dimensions.y;
    rgb = &g_wldcore_window_render_records[index].red;
    rgb[0] = 0x80;
    rgb[1] = 0x80;
    rgb[2] = 0x80;
    g_wldcore_window_render_records[index].anim_counter = 0;
}
