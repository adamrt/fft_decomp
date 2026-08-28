/* The typed views are casts at the use sites because copying them into
 * locals reorders the prologue's saved-register moves. One `scaled` local is
 * shared across both /200 divisions, as the target puts both results in $t3. */
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* The header owns the signature, so the typed views are spelled as casts at
 * the use sites rather than taken as typed parameters or copied into locals:
 * a cast of a pointer parameter is the parameter's own pseudo, which is what
 * a typed parameter would have given. */
void world_gfx_build_scaled_draw_area_pair_at_offset(
    void* packets, const void* geometry, s32 buffer, s32 frame_offset, void* thread) {
    s32 percent;
    s32 w;
    s32 h;
    s32 scaled;

    if (g_world_event_speed == 2) {
        buffer *= 2;
    }
    if (buffer >= 12) {
        buffer = 11;
    }
    percent = g_world_gfx_panel_draw_area_scale_percent[buffer];
    world_script_copy_bytes(&((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0], geometry, 8);
    w = ((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0].w;
    h = ((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0].h;
    ((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0].x -= 0x80;
    ((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0].x += ((const RECT*)thread)->x;
    ((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0].y += ((const RECT*)thread)->w;
    scaled = (w * percent) / 200;
    ((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0].x
        = w / 2 + ((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0].x - scaled;
    scaled = (h * percent) / 200;
    ((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0].y
        = h / 2 + ((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0].y - scaled;
    ((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0].w
        = (((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0].w * percent) / 100;
    ((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0].h
        = (((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0].h * percent) / 100;
    if (frame_offset != 0) {
        ((world_gfx_scaled_draw_area_pair_t*)packets)->rects[1].y = 0;
    } else {
        ((world_gfx_scaled_draw_area_pair_t*)packets)->rects[1].y = 0xF0;
        ((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0].y += 0xF0;
    }
    SetDrawArea(&((world_gfx_scaled_draw_area_pair_t*)packets)->areas[0],
        &((world_gfx_scaled_draw_area_pair_t*)packets)->rects[0]);
    SetDrawArea(&((world_gfx_scaled_draw_area_pair_t*)packets)->areas[1],
        &((world_gfx_scaled_draw_area_pair_t*)packets)->rects[1]);
}
