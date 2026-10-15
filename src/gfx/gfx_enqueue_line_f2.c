#include "fft/event_equip.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Draw the two endpoint coordinates with an x bias of 0x80. */
void gfx_enqueue_line_f2(const point16_t endpoints[2], const rgb24_t* color, u8 semitrans, s32 otag_index) {
    LINE_F2* line;

    line = &g_menu_gfx_context->flat_lines[g_equip_gfx_line_f2_count++];
    setRGB0(line, color->r, color->g, color->b);
    SetSemiTrans(line, semitrans);
    setXY2(line, endpoints[0].x + 0x80, endpoints[0].y, endpoints[1].x + 0x80, endpoints[1].y);
    addPrim(&g_menu_gfx_context->otag[otag_index], line);
}
