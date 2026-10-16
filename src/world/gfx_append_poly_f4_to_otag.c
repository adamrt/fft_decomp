/* Appending a flat-colored menu quad to an ordering-table entry. */

#include "fft/gfx.h"
#include "fft/menu_gfx.h"
#include "fft/world_gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Takes the next POLY_F4 from the pool, sets semi-transparency, colors it,
 * copies the four x/y pairs (x shifted by 0x80) and links it into
 * ordering-table entry ot_index (libgpu addPrim). */
void screen_append_poly_f4(point16_t* xy, rgb24_t* rgb, s32 semi_trans, s32 ot_index) {
    POLY_F4* poly = &g_menu_gfx_context->flat_quads[g_screen_poly_f4_count++];

    SetSemiTrans(poly, semi_trans);
    setRGB0(poly, rgb->r, rgb->g, rgb->b);
    setXY4(poly, xy[0].x + 0x80, xy[0].y, xy[1].x + 0x80, xy[1].y, xy[2].x + 0x80, xy[2].y, xy[3].x + 0x80, xy[3].y);
    addPrim(&g_menu_gfx_context->otag[ot_index], poly);
}
