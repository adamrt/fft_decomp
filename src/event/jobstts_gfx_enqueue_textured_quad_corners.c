#include "fft/jobstts.h"

typedef struct jobstts_quad_t {
    s16 x0;
    s16 y0;
    s16 x1;
    s16 y1;
    s16 x2;
    s16 y2;
    s16 x3;
    s16 y3;
    s16 u0;
    s16 v0;
    s16 u1;
    s16 v1;
    s16 u2;
    s16 v2;
    s16 u3;
    s16 v3;
    u16 clut;
    u16 tpage;
} jobstts_quad_t;

void jobstts_gfx_enqueue_textured_quad_corners(jobstts_quad_t* quad, u8* color, s32 semitrans, s32 ot_index) {
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

    poly->x0 = quad->x0 + 0x80;
    poly->y0 = quad->y0;
    poly->x1 = quad->x1 + 0x80;
    poly->y1 = quad->y1;
    poly->x2 = quad->x2 + 0x80;
    poly->y2 = quad->y2;
    poly->x3 = quad->x3 + 0x80;
    poly->y3 = quad->y3;
    poly->u0 = quad->u0;
    poly->v0 = quad->v0;
    poly->u1 = quad->u1;
    poly->v1 = quad->v1;
    poly->u2 = quad->u2;
    poly->v2 = quad->v2;
    poly->u3 = quad->u3;
    poly->v3 = quad->v3;
    poly->clut = quad->clut;
    poly->tpage = quad->tpage;

    ((P_TAG*)poly)->addr = g_jobstts_gfx_context->otag[ot_index];
    ((P_TAG*)&g_jobstts_gfx_context->otag[ot_index])->addr = (u32)poly;
}
