#include "fft/world.h"
#include "psx/gpu.h"

/*
 * Append a four-color menu quad to the selected ordering-table entry.
 *
 * Screen x coordinates use the menu's +0x80 origin. Packet length and
 * opcode are initialized by the pool owner and retained when linking.
 */
void world_gfx_append_poly_g4_to_otag(s16* xy, u8* rgb, s32 semi_trans, s32 ot_index) {
    POLY_G4* poly = &g_world_gfx_active_packet_buffer->gradient_quads[g_world_gfx_gradient_quad_count++];
    SetSemiTrans(poly, semi_trans);
    poly->x0 = xy[0] + 0x80;
    poly->y0 = xy[1];
    poly->x1 = xy[2] + 0x80;
    poly->y1 = xy[3];
    poly->x2 = xy[4] + 0x80;
    poly->y2 = xy[5];
    poly->x3 = xy[6] + 0x80;
    poly->y3 = xy[7];
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
    ((P_TAG*)poly)->addr = ((P_TAG*)&g_world_gfx_active_packet_buffer->otag[ot_index])->addr;
    ((P_TAG*)&g_world_gfx_active_packet_buffer->otag[ot_index])->addr = (u32)poly;
}
