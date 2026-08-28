#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Takes the next TILE from the pool, colours it, sets semi-transparency and
 * links it into ordering-table entry ot_index (libgpu addPrim). */
/* Target 0x8012bdbc. */
void world_gfx_append_tile_to_otag(RECT* box, u8* rgb, s32 semi_trans, s32 ot_index) {
    TILE* tile = &g_world_gfx_active_packet_buffer->tiles[g_world_gfx_tile_count++];

    tile->r0 = rgb[0];
    tile->g0 = rgb[1];
    tile->b0 = rgb[2];
    SetSemiTrans(tile, semi_trans & 0xFF);
    tile->x0 = box->x + 0x80;
    tile->y0 = box->y;
    tile->w = box->w;
    tile->h = box->h;
    setaddr(tile, getaddr(&g_world_gfx_active_packet_buffer->otag[ot_index]));
    setaddr(&g_world_gfx_active_packet_buffer->otag[ot_index], tile);
}
