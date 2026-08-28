#include "fft/equip.h"
#include "psx/types.h"

void equip_gfx_enqueue_textured_quad_current_ot(
    const RECT* rect, s32 u, s32 v, const u8* color, s32 semitrans, u16 texture_page, u16 clut) {
    POLY_FT4* poly;
    u16 index = g_equip_gfx_poly_ft4_count;

    g_equip_gfx_poly_ft4_count = index + 1;
    poly = &g_equip_gfx_context->textured_quads[index];

    if (color != 0) {
        SetShadeTex(poly, 0);
        poly->r0 = color[0];
        poly->g0 = color[1];
        poly->b0 = color[2];
    } else {
        SetShadeTex(poly, 1);
    }
    SetSemiTrans(poly, semitrans);

    poly->x0 = rect->x + 0x80;
    poly->y0 = rect->y;
    {
        s32 right_offset = (u16)rect->w + 0x80;
        s32 right_x = (u16)rect->x;
        right_x += right_offset;
        poly->x1 = right_x;
    }
    poly->y1 = rect->y;
    poly->x2 = rect->x + 0x80;
    poly->y2 = rect->y + rect->h;
    {
        s32 right_offset = (u16)rect->w + 0x80;
        s32 right_x = (u16)rect->x;
        right_x += right_offset;
        poly->x3 = right_x;
    }
    poly->y3 = rect->y + rect->h;

    poly->u0 = u;
    poly->v0 = v;
    poly->u1 = u + rect->w;
    poly->v1 = v;
    poly->u2 = u;
    poly->v2 = v + rect->h;
    poly->u3 = u + rect->w;
    poly->v3 = v + rect->h;
    poly->clut = clut;
    poly->tpage = texture_page;

    {
        u32* ot = g_equip_gfx_current_ot;
        setaddr(poly, getaddr(ot));
        setaddr(ot, poly);
    }
}
