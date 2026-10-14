#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Center the two job-wheel portrait quads on (0x80, 0x7b) for the selected
 * unit's formation graphic and queue a VRAM move of that graphic to
 * (0x240, 0x130), where g_world_job_wheel_portrait_quad samples it. */
void world_formation_layout_job_wheel_portrait(void) {
    texture_source_t entry;
    RECT rect;
    s16 x;
    s16 y;

    world_formation_build_unit_graphic_entry(g_world_formation_selected_unit_index, &entry, 0);
    x = 0x80 - (entry.w >> 1);
    y = 0x7b - (entry.h >> 1);
    g_world_job_wheel_portrait_quad.y = y;
    g_world_job_wheel_new_portrait_quad.y = y;
    g_world_job_wheel_portrait_quad.x = x;
    g_world_job_wheel_new_portrait_quad.x = x;
    g_world_job_wheel_portrait_quad.clut = entry.clut;
    if (entry.tpage == 0x64) {
        rect.x = (entry.u >> 2) + 0x100;
    } else {
        rect.x = (entry.u >> 2) + 0x140;
    }
    rect.y = entry.v;
    rect.w = entry.w >> 2;
    rect.h = entry.h;
    world_gfx_enqueue_draw_move(&rect, 0x240, 0x130, 0);
}
