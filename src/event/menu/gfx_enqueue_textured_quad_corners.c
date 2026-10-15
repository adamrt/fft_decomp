#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/types.h"

/*
 * Append a textured menu quad, given by its four corners, to the selected
 * ordering-table entry.
 *
 * Screen x coordinates use the menu's +0x80 origin. With `color` the quad is
 * shaded with that color; otherwise texture shading is disabled.
 */
void gfx_enqueue_textured_quad_corners(const textured_quad_source_t* src, const rgb24_t* color, s32 semi, s32 idx) {
    POLY_FT4* poly;

    poly = &g_menu_gfx_context->textured_quads[g_menu_gfx_poly_ft4_count++];
    if (color != 0) {
        SetShadeTex(poly, 0);
        setRGB0(poly, color->r, color->g, color->b);
    } else {
        SetShadeTex(poly, 1);
    }
    SetSemiTrans(poly, semi);
    setXY4(poly, src->x0 + 0x80, src->y0, src->x1 + 0x80, src->y1, src->x2 + 0x80, src->y2, src->x3 + 0x80, src->y3);
    setUV4(poly, src->u0, src->v0, src->u1, src->v1, src->u2, src->v2, src->u3, src->v3);
    poly->clut = src->clut;
    poly->tpage = src->tpage;
    addPrim(&g_menu_gfx_context->otag[idx], poly);
}
