#include "fft/jobstts.h"

typedef struct jobstts_sprite {
    u8 x;
    u8 y;
    u8 w;
    u8 h;
    u8 u;
    u8 v;
} jobstts_sprite_t;

void jobstts_gfx_enqueue_textured_quad_list(
    jobstts_sprite_t* sprites, u8* color, s32 semitrans, u16 tpage, u16 clut, s32 ot_index, s32 count) {
    POLY_FT4* poly;

    for (count -= 1; count >= 0; count--) {
        poly = &g_jobstts_gfx_context->textured_quads[g_jobstts_gfx_poly_ft4_count++];

        if (color != 0) {
            SetShadeTex(poly, 0);
            poly->r0 = color[0];
            poly->g0 = color[1];
            poly->b0 = color[2];
        } else {
            SetShadeTex(poly, 1);
        }
        SetSemiTrans(poly, semitrans);

        poly->x0 = sprites->x + 0x80;
        poly->y0 = sprites->y;
        poly->x1 = sprites->w + (sprites->x + 0x80);
        poly->y1 = sprites->y;
        poly->x2 = sprites->x + 0x80;
        poly->y2 = sprites->y + sprites->h;
        poly->x3 = sprites->w + (sprites->x + 0x80);
        poly->y3 = sprites->y + sprites->h;
        poly->u0 = sprites->u;
        poly->v0 = sprites->v;
        poly->u1 = sprites->u + sprites->w;
        poly->v1 = sprites->v;
        poly->u2 = sprites->u;
        poly->v2 = sprites->v + sprites->h;
        poly->u3 = sprites->u + sprites->w;
        poly->v3 = sprites->v + sprites->h;
        poly->clut = clut;
        poly->tpage = tpage;

        ((P_TAG*)poly)->addr = g_jobstts_gfx_context->otag[ot_index];
        ((P_TAG*)&g_jobstts_gfx_context->otag[ot_index])->addr = (u32)poly;
        sprites++;
    }
}
