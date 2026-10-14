#include "fft/event_card.h"
#include "psx/types.h"

/* Takes one POLY_FT4 from the pool per sprite cell, fills it as a w x h quad
 * at (x + 0x80, y) with texture origin (u, v), and links it into ordering
 * table entry otag_index. With `color` the quads are shaded with that
 * color. */
void card_gfx_enqueue_textured_quad_list(const menu_sprite_cell_t* quads, const rgb24_t* color, s32 semitrans,
    u16 texture_page, u16 clut, s32 otag_index, s32 count) {
    POLY_FT4* poly;

    for (count -= 1; count >= 0; count--) {
        poly = &g_card_gfx_context->polygons[g_card_gfx_poly_ft4_count++];

        if (color != 0) {
            SetShadeTex(poly, 0);
            setRGB0(poly, color->r, color->g, color->b);
        } else {
            SetShadeTex(poly, 1);
        }
        SetSemiTrans(poly, semitrans);

        setXY4(poly, quads->x + 0x80, quads->y, quads->w + (quads->x + 0x80), quads->y, quads->x + 0x80,
            quads->y + quads->h, quads->w + (quads->x + 0x80), quads->y + quads->h);
        setUV4(poly, quads->u, quads->v, quads->u + quads->w, quads->v, quads->u, quads->v + quads->h,
            quads->u + quads->w, quads->v + quads->h);
        poly->clut = clut;
        poly->tpage = texture_page;

        setaddr(poly, g_card_gfx_context->otag[otag_index]);
        catPrim(&g_card_gfx_context->otag[otag_index], poly);
        quads++;
    }
}
