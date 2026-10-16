/* Appending a four-color menu quad to an ordering-table entry. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Append a four-color menu quad to the selected ordering-table entry.
 *
 * `pts` holds the four corners as x, y pairs and `color` four RGB triples.
 * Screen x coordinates use the menu's +0x80 origin.
 */
void gfx_enqueue_gouraud_quad(const upoint16_t* pts, const rgb24_t* color, s32 semi, s32 idx) {
    POLY_G4* poly = &g_menu_gfx_context->gouraud_quads[g_gfx_poly_g4_count++];

    SetSemiTrans(poly, semi);
    setXY4(poly, pts[0].x + 0x80, pts[0].y, pts[1].x + 0x80, pts[1].y, pts[2].x + 0x80, pts[2].y, pts[3].x + 0x80,
        pts[3].y);
    setRGB0(poly, color[0].r, color[0].g, color[0].b);
    setRGB1(poly, color[1].r, color[1].g, color[1].b);
    setRGB2(poly, color[2].r, color[2].g, color[2].b);
    setRGB3(poly, color[3].r, color[3].g, color[3].b);
    addPrim(&g_menu_gfx_context->otag[idx], poly);
}
