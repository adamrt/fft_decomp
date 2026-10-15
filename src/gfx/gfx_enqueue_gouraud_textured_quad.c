#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/types.h"

/*
 * Append a Gouraud-shaded textured rectangle to the selected ordering-table
 * entry. Screen x coordinates use the menu's +0x80 origin; `color` holds four
 * RGB triples, one per corner.
 */
void gfx_enqueue_gouraud_textured_quad(
    const textured_rect_source_t* rect, const rgb24_t* color, s32 semitrans, s32 otag_index) {
    POLY_GT4* poly;

    poly = &g_menu_gfx_context->gouraud_textured_quads[g_menu_gfx_poly_gt4_count++];

    SetSemiTrans(poly, semitrans);

    setXY4(poly, rect->x + 0x80, rect->y, (rect->x + 0x80) + rect->w, rect->y, rect->x + 0x80, rect->y + rect->h,
        (rect->x + 0x80) + rect->w, rect->y + rect->h);

    setUV4(poly, rect->u, rect->v, rect->u + rect->w, rect->v, rect->u, rect->v + rect->h, rect->u + rect->w,
        rect->v + rect->h);

    setRGB0(poly, color[0].r, color[0].g, color[0].b);
    setRGB1(poly, color[1].r, color[1].g, color[1].b);
    setRGB2(poly, color[2].r, color[2].g, color[2].b);
    setRGB3(poly, color[3].r, color[3].g, color[3].b);

    poly->clut = rect->clut;
    poly->tpage = rect->tpage;

    addPrim(&g_menu_gfx_context->otag[otag_index], poly);
}
