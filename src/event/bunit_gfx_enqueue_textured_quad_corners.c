#include "fft/bunit.h"
#include "psx/types.h"

void bunit_gfx_enqueue_textured_quad_corners(bunit_sprite_desc_t* src, u8* color, s32 semi, s32 idx) {
    POLY_FT4* poly;

    poly = &g_bunit_gfx_context->textured_quads[g_bunit_gfx_poly_ft4_count++];
    if (color != 0) {
        SetShadeTex(poly, 0);
        poly->r0 = color[0];
        poly->g0 = color[1];
        poly->b0 = color[2];
    } else {
        SetShadeTex(poly, 1);
    }
    SetSemiTrans(poly, semi);
    poly->x0 = src->x0 + 0x80;
    poly->y0 = src->y0;
    poly->x1 = src->x1 + 0x80;
    poly->y1 = src->y1;
    poly->x2 = src->x2 + 0x80;
    poly->y2 = src->y2;
    poly->x3 = src->x3 + 0x80;
    poly->y3 = src->y3;
    poly->u0 = src->u0;
    poly->v0 = src->v0;
    poly->u1 = src->u1;
    poly->v1 = src->v1;
    poly->u2 = src->u2;
    poly->v2 = src->v2;
    poly->u3 = src->u3;
    poly->v3 = src->v3;
    poly->clut = src->clut;
    poly->tpage = src->tpage;
    {
        bunit_gfx_context_t* ctx = g_bunit_gfx_context;
        setaddr(poly, getaddr(&ctx->otag[idx]));
        setaddr(&ctx->otag[idx], poly);
    }
}
