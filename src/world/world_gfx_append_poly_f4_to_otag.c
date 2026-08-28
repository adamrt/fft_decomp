#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Takes the next POLY_F4 from the pool, sets semi-transparency, colours it,
 * copies the four x/y pairs (x shifted by 0x80) and links it into
 * ordering-table entry ot_index (libgpu addPrim). */
/* Target 0x8012bebc. */
void world_gfx_append_poly_f4_to_otag(s16* xy, u8* rgb, s32 semi_trans, s32 ot_index) {
    POLY_F4* poly = &g_world_gfx_active_packet_buffer->poly_f4s[g_world_gfx_poly_f4_count++];

    SetSemiTrans(poly, semi_trans);
    poly->r0 = rgb[0];
    poly->g0 = rgb[1];
    poly->b0 = rgb[2];
    poly->x0 = xy[0] + 0x80;
    poly->y0 = xy[1];
    poly->x1 = xy[2] + 0x80;
    poly->y1 = xy[3];
    poly->x2 = xy[4] + 0x80;
    poly->y2 = xy[5];
    poly->x3 = xy[6] + 0x80;
    poly->y3 = xy[7];
    setaddr(poly, getaddr(&g_world_gfx_active_packet_buffer->otag[ot_index]));
    setaddr(&g_world_gfx_active_packet_buffer->otag[ot_index], poly);
}
