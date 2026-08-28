#include "fft/world.h"
#include "psx/gpu.h"

/*
 * Append a Gouraud-shaded textured rectangle to the selected ordering-table
 * entry. Screen x coordinates use the menu's +0x80 origin; `rgb` holds four
 * RGB triples, one per corner.
 */
void world_gfx_append_poly_gt4_to_otag(
    world_gfx_textured_rect_source_t* source, u8* rgb, s32 semi_trans, s32 ot_index) {
    POLY_GT4* poly
        = &g_world_gfx_active_packet_buffer->textured_gradient_quads[g_world_gfx_textured_gradient_quad_count++];

    SetSemiTrans(poly, semi_trans);
    poly->x0 = source->x + 0x80;
    poly->y0 = source->y;
    poly->x1 = (source->x + 0x80) + source->w;
    poly->y1 = source->y;
    poly->x2 = source->x + 0x80;
    poly->y2 = source->y + source->h;
    poly->x3 = (source->x + 0x80) + source->w;
    poly->y3 = source->y + source->h;
    poly->u0 = source->u;
    poly->v0 = source->v;
    poly->u1 = source->u + source->w;
    poly->v1 = source->v;
    poly->u2 = source->u;
    poly->v2 = source->v + source->h;
    poly->u3 = source->u + source->w;
    poly->v3 = source->v + source->h;
    poly->r0 = rgb[0];
    poly->g0 = rgb[1];
    poly->b0 = rgb[2];
    poly->r1 = rgb[3];
    poly->g1 = rgb[4];
    poly->b1 = rgb[5];
    poly->r2 = rgb[6];
    poly->g2 = rgb[7];
    poly->b2 = rgb[8];
    poly->r3 = rgb[9];
    poly->g3 = rgb[10];
    poly->b3 = rgb[11];
    poly->clut = source->clut;
    poly->tpage = source->tpage;
    ((P_TAG*)poly)->addr = ((P_TAG*)&g_world_gfx_active_packet_buffer->otag[ot_index])->addr;
    ((P_TAG*)&g_world_gfx_active_packet_buffer->otag[ot_index])->addr = (u32)poly;
}
