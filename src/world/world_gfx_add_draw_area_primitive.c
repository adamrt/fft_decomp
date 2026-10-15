#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Takes the next DR_AREA packet from the pool, fills it from rect (moved to
 * the second frame buffer when g_world_gfx_draw_buffer_clip_y >= 0x65) and links it into
 * ordering-table entry ot_index (libgpu addPrim). */
void world_gfx_add_draw_area_primitive(RECT* rect, s32 ot_index) {
    DR_AREA* area;

    if (g_world_gfx_draw_buffer_clip_y >= 0x65) {
        rect->y += 0xF0;
    }
    area = &g_menu_gfx_context->draw_areas[g_world_gfx_draw_area_count++];
    SetDrawArea(area, rect);
    addPrim(&g_menu_gfx_context->otag[ot_index], area);
}
