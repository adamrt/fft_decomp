#include "fft/event_jobstts.h"

void jobstts_gfx_enqueue_textured_quad(
    urect16_t* rect, s32 u, s32 v, u8* color, s32 semitrans, u16 tpage, u16 clut, s32 ot_index) {
    POLY_FT4* poly;

    poly = &g_jobstts_gfx_context->textured_quads[g_jobstts_gfx_poly_ft4_count++];

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
    poly->x1 = rect->x + (rect->w + 0x80);
    poly->y1 = rect->y;
    poly->x2 = rect->x + 0x80;
    poly->y2 = rect->y + rect->h;
    poly->x3 = rect->x + (rect->w + 0x80);
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
    poly->tpage = tpage;

    ((P_TAG*)poly)->addr = g_jobstts_gfx_context->otag[ot_index];
    ((P_TAG*)&g_jobstts_gfx_context->otag[ot_index])->addr = (u32)poly;
}
