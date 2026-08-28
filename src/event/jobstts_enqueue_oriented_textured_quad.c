#include "fft/jobstts.h"
#include "fft/main_gfx.h"

/* Texture cell for jobstts_enqueue_oriented_textured_quad; the same layout
 * as the tail of EQUIP/BUNIT's oriented-quad descriptors. */
typedef struct jobstts_oriented_uv {
    u8 u;
    u8 pad_01;
    u8 v;
    u8 pad_03;
    u8 uw;
    u8 pad_05;
    u8 vh;
    u16 clut;  /* 0x08 */
    u16 tpage; /* 0x0a */
} jobstts_oriented_uv_t;

/* Queue a textured quad whose texture cell is mirrored by orientation. */
void jobstts_enqueue_oriented_textured_quad(
    urect16_t* rect, jobstts_oriented_uv_t* uv, u8* color, s32 orientation, s32 semitrans, s32 ot_index) {
    s32 quad_semitrans = semitrans;
    s32 quad_orientation = orientation;
    s32 quad_ot_index = ot_index;
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
    SetSemiTrans(poly, quad_semitrans);

    poly->x0 = rect->x + 0x80;
    poly->y0 = rect->y;
    poly->x1 = rect->x + (rect->w + 0x80);
    poly->y1 = rect->y;
    poly->x2 = rect->x + 0x80;
    poly->y2 = rect->y + rect->h;
    poly->x3 = rect->x + (rect->w + 0x80);
    poly->y3 = rect->y + rect->h;

    switch (quad_orientation & 0xff) {
    case TEXTURE_UV_FLIP_NONE:
        poly->u0 = uv->u;
        poly->v0 = uv->v;
        poly->u1 = uv->u + uv->uw;
        poly->v1 = uv->v;
        poly->u2 = uv->u;
        poly->v2 = uv->v + uv->vh;
        poly->u3 = uv->u + uv->uw;
        poly->v3 = uv->v + uv->vh;
        break;
    case TEXTURE_UV_FLIP_X:
        poly->u1 = uv->u;
        poly->v1 = uv->v;
        poly->u0 = uv->u + uv->uw;
        poly->v0 = uv->v;
        poly->u3 = uv->u;
        poly->v3 = uv->v + uv->vh;
        poly->u2 = uv->u + uv->uw;
        poly->v2 = uv->v + uv->vh;
        break;
    case TEXTURE_UV_FLIP_Y:
        poly->u2 = uv->u;
        poly->v2 = uv->v;
        poly->u3 = uv->u + uv->uw;
        poly->v3 = uv->v;
        poly->u0 = uv->u;
        poly->v0 = uv->v + uv->vh;
        poly->u1 = uv->u + uv->uw;
        poly->v1 = uv->v + uv->vh;
        break;
    case TEXTURE_UV_FLIP_XY:
        poly->u3 = uv->u;
        poly->v3 = uv->v;
        poly->u2 = uv->u + uv->uw;
        poly->v2 = uv->v;
        poly->u1 = uv->u;
        poly->v1 = uv->v + uv->vh;
        poly->u0 = uv->u + uv->uw;
        poly->v0 = uv->v + uv->vh;
        break;
    }

    poly->clut = uv->clut;
    poly->tpage = uv->tpage;

    ((P_TAG*)poly)->addr = g_jobstts_gfx_context->otag[quad_ot_index];
    ((P_TAG*)&g_jobstts_gfx_context->otag[quad_ot_index])->addr = (u32)poly;
}
