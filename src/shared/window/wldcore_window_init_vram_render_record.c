#include "fft/wldcore.h"

/* Prepare a render record for an image that is already in VRAM.
 *
 * Caller flags replace bits 0x18; texture coordinates use doubled 4bpp units.
 */
void wldcore_window_init_vram_render_record(s32 index, RECT bounds, point32_t clut, s32 flags) {
    RECT* record_bounds;
    CVECTOR* rgb;

    g_wldcore_window_render_records[index].flags
        = (g_wldcore_window_render_records[index].flags & ~(WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN))
        | flags;
    g_wldcore_window_render_records[index].clut = clut;
    g_wldcore_window_render_records[index].tpage = GetTPage(0, 0, bounds.x, bounds.y);
    record_bounds = &g_wldcore_window_render_records[index].bounds;
    record_bounds->x = (bounds.x & 0x3F) * 2;
    record_bounds->y = bounds.y & 0xFF;
    record_bounds->w = bounds.w * 2;
    record_bounds->h = bounds.h;
    rgb = &g_wldcore_window_render_records[index].color;
    rgb->b = rgb->g = rgb->r = 0x80;
    g_wldcore_window_render_records[index].anim_counter = 0;
}
