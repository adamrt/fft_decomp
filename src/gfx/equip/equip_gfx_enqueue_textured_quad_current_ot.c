#include "fft/event_equip.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Takes one POLY_FT4 from the pool, fills it as the rect->w x rect->h quad at
 * (rect->x + 0x80, rect->y) with texture origin (u, v), and links it at the
 * ordering-table entry g_equip_gfx_current_ot points to; the last argument
 * is not read. With `color` the quad is shaded with it. */
void equip_gfx_enqueue_textured_quad_current_ot(const RECT* rect, s32 u, s32 v, const rgb24_t* color, s32 semitrans,
    u16 texture_page, u16 clut, s32 unused_priority) {
    POLY_FT4* poly;

    poly = &g_menu_gfx_context->textured_quads[g_menu_gfx_poly_ft4_count++];

    if (color != 0) {
        SetShadeTex(poly, 0);
        setRGB0(poly, color->r, color->g, color->b);
    } else {
        SetShadeTex(poly, 1);
    }
    SetSemiTrans(poly, semitrans);

    setXYWH(poly, rect->x + 0x80, rect->y, rect->w, rect->h);
    setUVWH(poly, u, v, rect->w, rect->h);
    poly->clut = clut;
    poly->tpage = texture_page;

    {
        u32* ot = g_equip_gfx_current_ot;
        addPrim(ot, poly);
    }
}
