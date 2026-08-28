#include "fft/world.h"
#include "psx/gpu.h"

/*
 * Append a textured menu quad to the selected ordering-table entry.
 *
 * Screen x coordinates use the menu's +0x80 origin. With `rgb` the quad is
 * shaded with that color; otherwise texture shading is disabled.
 */
void world_gfx_append_poly_ft4_to_otag(
    world_gfx_textured_quad_source_t* source, u8* rgb, s32 semi_trans, s32 ot_index) {
    POLY_FT4* poly = &g_world_gfx_active_packet_buffer->textured_quads[g_world_gfx_textured_quad_count++];
    if (rgb != 0) {
        SetShadeTex(poly, 0);
        poly->r0 = rgb[0];
        poly->g0 = rgb[1];
        poly->b0 = rgb[2];
    } else {
        SetShadeTex(poly, 1);
    }
    SetSemiTrans(poly, semi_trans);
    poly->x0 = source->x0 + 0x80;
    poly->y0 = source->y0;
    poly->x1 = source->x1 + 0x80;
    poly->y1 = source->y1;
    poly->x2 = source->x2 + 0x80;
    poly->y2 = source->y2;
    poly->x3 = source->x3 + 0x80;
    poly->y3 = source->y3;
    poly->u0 = source->u0;
    poly->v0 = source->v0;
    poly->u1 = source->u1;
    poly->v1 = source->v1;
    poly->u2 = source->u2;
    poly->v2 = source->v2;
    poly->u3 = source->u3;
    poly->v3 = source->v3;
    poly->clut = source->clut;
    poly->tpage = source->tpage;
    ((P_TAG*)poly)->addr = ((P_TAG*)&g_world_gfx_active_packet_buffer->otag[ot_index])->addr;
    ((P_TAG*)&g_world_gfx_active_packet_buffer->otag[ot_index])->addr = (u32)poly;
}
