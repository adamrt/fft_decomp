#include "fft/event_jobstts.h"

typedef struct jobstts_tex_region {
    u16 u;
    u16 v;
    u16 w;
    u16 h;
    u16 clut;
    u16 tpage;
} jobstts_tex_region_t;

void jobstts_gfx_enqueue_textured_quad_rect(
    urect16_t* rect, jobstts_tex_region_t* tex, u8* color, s32 semitrans, s32 ot_index) {
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
    poly->u0 = tex->u;
    poly->v0 = tex->v;
    poly->u1 = tex->u + tex->w;
    poly->v1 = tex->v;
    poly->u2 = tex->u;
    poly->v2 = tex->v + tex->h;
    poly->u3 = tex->u + tex->w;
    poly->v3 = tex->v + tex->h;
    poly->clut = tex->clut;
    poly->tpage = tex->tpage;

    ((P_TAG*)poly)->addr = g_jobstts_gfx_context->otag[ot_index];
    ((P_TAG*)&g_jobstts_gfx_context->otag[ot_index])->addr = (u32)poly;
}
