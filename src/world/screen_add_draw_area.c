/* Queuing a drawing area primitive, moved to the second frame buffer when needed. */

#include "fft/menu_gfx.h"
#include "fft/screen.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Takes the next DR_AREA packet from the pool, fills it from rect and links
 * it into ordering-table entry ot_index (libgpu addPrim).
 *
 * A drawing area is in absolute VRAM coordinates, so while the lower frame
 * buffer is the draw target (clip y 240 rather than 0) the rect moves down
 * 240 lines. The caller's rect is changed in place. */
void screen_add_draw_area(RECT* rect, s32 ot_index) {
    DR_AREA* area;

    if (g_screen_draw_buffer_clip_y >= 0x65) {
        rect->y += 0xF0;
    }
    area = &g_menu_gfx_context->draw_areas[g_screen_draw_area_count++];
    SetDrawArea(area, rect);
    addPrim(&g_menu_gfx_context->otag[ot_index], area);
}
