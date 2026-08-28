#include "fft/world.h"
#include "psx/gpu.h"

/* DRAWENV setup reads these as x/y/w/h. The casts keep
 * the target's unsigned halfword loads without changing SDK RECT. */
void world_gfx_set_draw_clip_rectangle(const volatile RECT* source) {
    g_world_gs_clip_rect.x = (u16)source->x;
    g_world_gs_clip_rect.y = (u16)source->y;
    g_world_gs_clip_rect.w = (u16)source->w;
    g_world_gs_clip_rect.h = (u16)source->h;
}
