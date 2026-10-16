#include "fft/event_jobstts.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Queue a textured quad whose texture cell is mirrored by orientation. */
void jobstts_gfx_enqueue_oriented_textured_quad(
    RECT* rect, texture_source_t* uv, rgb24_t* color, s32 orientation, s32 semitrans, s32 ot_index) {
    s32 quad_semitrans = semitrans;
    s32 quad_orientation = orientation;
    s32 quad_ot_index = ot_index;
    POLY_FT4* poly;

    poly = &g_menu_gfx_context->textured_quads[g_menu_gfx_poly_ft4_count++];

    if (color != 0) {
        SetShadeTex(poly, 0);
        setRGB0(poly, color->r, color->g, color->b);
    } else {
        SetShadeTex(poly, 1);
    }
    SetSemiTrans(poly, quad_semitrans);

    setXYWH(poly, rect->x + 0x80, rect->y, rect->w, rect->h);

    switch (quad_orientation & 0xff) {
    case TEXTURE_UV_FLIP_NONE:
        setUV4(poly, uv->u, uv->v, uv->u + uv->w, uv->v, uv->u, uv->v + uv->h, uv->u + uv->w, uv->v + uv->h);
        break;
    case TEXTURE_UV_FLIP_X:
        poly->u1 = uv->u;
        poly->v1 = uv->v;
        poly->u0 = uv->u + uv->w;
        poly->v0 = uv->v;
        poly->u3 = uv->u;
        poly->v3 = uv->v + uv->h;
        poly->u2 = uv->u + uv->w;
        poly->v2 = uv->v + uv->h;
        break;
    case TEXTURE_UV_FLIP_Y:
        poly->u2 = uv->u;
        poly->v2 = uv->v;
        poly->u3 = uv->u + uv->w;
        poly->v3 = uv->v;
        poly->u0 = uv->u;
        poly->v0 = uv->v + uv->h;
        poly->u1 = uv->u + uv->w;
        poly->v1 = uv->v + uv->h;
        break;
    case TEXTURE_UV_FLIP_XY:
        poly->u3 = uv->u;
        poly->v3 = uv->v;
        poly->u2 = uv->u + uv->w;
        poly->v2 = uv->v;
        poly->u1 = uv->u;
        poly->v1 = uv->v + uv->h;
        poly->u0 = uv->u + uv->w;
        poly->v0 = uv->v + uv->h;
        break;
    }

    poly->clut = uv->clut;
    poly->tpage = uv->tpage;

    setaddr(poly, g_menu_gfx_context->otag[quad_ot_index]);
    catPrim(&g_menu_gfx_context->otag[quad_ot_index], poly);
}
