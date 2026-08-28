#include "fft/bunit.h"
#include "psx/types.h"

void bunit_gfx_enqueue_gouraud_textured_quad(
    const bunit_gouraud_rect_t* rect, const u8* color, s32 semitrans, s32 otag_index) {
    POLY_GT4* poly;
    u16 index = g_bunit_gfx_poly_gt4_count;

    g_bunit_gfx_poly_gt4_count = index + 1;
    poly = &g_bunit_gfx_context->gouraud_textured_quads[index];

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

    poly->u0 = rect->u;
    poly->v0 = rect->v;
    poly->u1 = rect->u + rect->w;
    poly->v1 = rect->v;
    poly->u2 = rect->u;
    poly->v2 = rect->v + rect->h;
    poly->u3 = rect->u + rect->w;
    poly->v3 = rect->v + rect->h;

    poly->r0 = color[0];
    poly->g0 = color[1];
    poly->b0 = color[2];
    poly->r1 = color[3];
    poly->g1 = color[4];
    poly->b1 = color[5];
    poly->r2 = color[6];
    poly->g2 = color[7];
    poly->b2 = color[8];
    poly->r3 = color[9];
    poly->g3 = color[10];
    poly->b3 = color[11];

    poly->clut = rect->clut;
    poly->tpage = rect->tpage;

    {
        bunit_gfx_context_t* context = g_bunit_gfx_context;
        setaddr(poly, getaddr(&context->otag[otag_index]));
        setaddr(&context->otag[otag_index], poly);
    }
}
