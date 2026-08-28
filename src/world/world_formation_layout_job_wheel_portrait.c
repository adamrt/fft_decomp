#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

void world_formation_layout_job_wheel_portrait(void) {
    world_formation_graphic_entry_t entry;
    RECT rect;
    s16 x;
    s16 y;

    world_formation_build_unit_graphic_entry(g_world_formation_selected_unit_index, &entry, 0);
    x = 0x80 - ((s16)entry.w >> 1);
    y = 0x7b - ((s16)entry.h >> 1);
    g_world_job_wheel_portrait_quad.y = y;
    g_world_job_wheel_new_portrait_quad.y = y;
    g_world_job_wheel_portrait_quad.x = x;
    g_world_job_wheel_new_portrait_quad.x = x;
    g_world_job_wheel_portrait_quad.clut = entry.clut;
    if (entry.tpage == 0x64) {
        rect.x = ((s16)entry.x >> 2) + 0x100;
    } else {
        rect.x = ((s16)entry.x >> 2) + 0x140;
    }
    rect.y = entry.y;
    rect.w = (s16)entry.w >> 2;
    rect.h = entry.h;
    world_gfx_add_draw_move_primitive(&rect, 0x240, 0x130, 0);
}
