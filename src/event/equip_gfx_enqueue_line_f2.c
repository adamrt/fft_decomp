#include "fft/equip.h"
#include "psx/types.h"

/* `rect` carries the two endpoints as (x0, y0, x1, y1). */
void equip_gfx_enqueue_line_f2(const RECT* rect, const u8* color, s32 semitrans, s32 otag_index) {
    LINE_F2* line;
    const RECT* endpoints = rect;
    u16 index = g_equip_gfx_line_f2_count;

    g_equip_gfx_line_f2_count = index + 1;
    line = &g_equip_gfx_context->lines[index];
    line->r0 = color[0];
    line->g0 = color[1];
    line->b0 = color[2];
    SetSemiTrans(line, semitrans & 0xff);
    line->x0 = endpoints->x + 0x80;
    line->y0 = endpoints->y;
    line->x1 = endpoints->w + 0x80;
    line->y1 = endpoints->h;
    {
        equip_gfx_context_t* context = g_equip_gfx_context;
        setaddr(line, getaddr(&context->otag[otag_index]));
        setaddr(&context->otag[otag_index], line);
    }
}
