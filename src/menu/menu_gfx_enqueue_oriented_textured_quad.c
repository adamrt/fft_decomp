#include "fft/menu.h"

/* Queue a textured quad for `descriptor` at ordering-table entry `otag_index`,
 * its texture cell mirrored by `orientation` (TEXTURE_UV_FLIP_*). A non-null
 * `color` modulates the texture with that RGB triple; the screen x is offset
 * by 0x80. */
void menu_gfx_enqueue_oriented_textured_quad(
    const oriented_quad_t* descriptor, const rgb24_t* color, s32 orientation, s32 semitrans, s32 otag_index) {
    s32 quad_semitrans = semitrans;
    s32 quad_orientation = orientation;
    POLY_FT4* poly;

    poly = &g_menu_gfx_context->textured_quads[g_menu_gfx_poly_ft4_count++];

    if (color != 0) {
        SetShadeTex(poly, 0);
        setRGB0(poly, color->r, color->g, color->b);
    } else {
        SetShadeTex(poly, 1);
    }
    SetSemiTrans(poly, quad_semitrans);

    setXY4(poly, descriptor->position.x + 0x80, descriptor->position.y, descriptor->position.x + (descriptor->w + 0x80),
        descriptor->position.y, descriptor->position.x + 0x80, descriptor->position.y + descriptor->h,
        descriptor->position.x + (descriptor->w + 0x80), descriptor->position.y + descriptor->h);

    switch (quad_orientation & 0xff) {
    case TEXTURE_UV_FLIP_NONE:
        setUV4(poly, descriptor->source.u, descriptor->source.v, descriptor->source.u + descriptor->source.w,
            descriptor->source.v, descriptor->source.u, descriptor->source.v + descriptor->source.h,
            descriptor->source.u + descriptor->source.w, descriptor->source.v + descriptor->source.h);
        break;
    case TEXTURE_UV_FLIP_X:
        poly->u1 = descriptor->source.u;
        poly->v1 = descriptor->source.v;
        poly->u0 = descriptor->source.u + descriptor->source.w;
        poly->v0 = descriptor->source.v;
        poly->u3 = descriptor->source.u;
        poly->v3 = descriptor->source.v + descriptor->source.h;
        poly->u2 = descriptor->source.u + descriptor->source.w;
        poly->v2 = descriptor->source.v + descriptor->source.h;
        break;
    case TEXTURE_UV_FLIP_Y:
        poly->u2 = descriptor->source.u;
        poly->v2 = descriptor->source.v;
        poly->u3 = descriptor->source.u + descriptor->source.w;
        poly->v3 = descriptor->source.v;
        poly->u0 = descriptor->source.u;
        poly->v0 = descriptor->source.v + descriptor->source.h;
        poly->u1 = descriptor->source.u + descriptor->source.w;
        poly->v1 = descriptor->source.v + descriptor->source.h;
        break;
    case TEXTURE_UV_FLIP_XY:
        poly->u3 = descriptor->source.u;
        poly->v3 = descriptor->source.v;
        poly->u2 = descriptor->source.u + descriptor->source.w;
        poly->v2 = descriptor->source.v;
        poly->u1 = descriptor->source.u;
        poly->v1 = descriptor->source.v + descriptor->source.h;
        poly->u0 = descriptor->source.u + descriptor->source.w;
        poly->v0 = descriptor->source.v + descriptor->source.h;
        break;
    }

    poly->clut = descriptor->source.clut;
    poly->tpage = descriptor->source.tpage;
    addPrim(&g_menu_gfx_context->otag[otag_index], poly);
}
