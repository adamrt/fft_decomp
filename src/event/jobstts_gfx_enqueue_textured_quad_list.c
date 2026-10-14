#include "fft/event_jobstts.h"

/* Takes one POLY_FT4 from the pool per sprite cell, fills it as a w x h quad
 * at (x + 0x80, y) with texture origin (u, v), and links it into ordering
 * table entry ot_index. With `color` the quads are shaded with that
 * color. */
void jobstts_gfx_enqueue_textured_quad_list(
    menu_sprite_cell_t* sprites, rgb24_t* color, s32 semitrans, u16 tpage, u16 clut, s32 ot_index, s32 count) {
    POLY_FT4* poly;

    for (count -= 1; count >= 0; count--) {
        poly = &g_jobstts_gfx_context->textured_quads[g_jobstts_gfx_poly_ft4_count++];

        if (color != 0) {
            SetShadeTex(poly, 0);
            setRGB0(poly, color->r, color->g, color->b);
        } else {
            SetShadeTex(poly, 1);
        }
        SetSemiTrans(poly, semitrans);

        setXY4(poly, sprites->x + 0x80, sprites->y, sprites->w + (sprites->x + 0x80), sprites->y, sprites->x + 0x80,
            sprites->y + sprites->h, sprites->w + (sprites->x + 0x80), sprites->y + sprites->h);
        setUV4(poly, sprites->u, sprites->v, sprites->u + sprites->w, sprites->v, sprites->u, sprites->v + sprites->h,
            sprites->u + sprites->w, sprites->v + sprites->h);
        poly->clut = clut;
        poly->tpage = tpage;

        setaddr(poly, g_jobstts_gfx_context->otag[ot_index]);
        catPrim(&g_jobstts_gfx_context->otag[ot_index], poly);
        sprites++;
    }
}
