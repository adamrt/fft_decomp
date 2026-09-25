#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Takes the next TILE packet from the pool, fills it from rect/rgb and links
 * it into ordering-table entry ot_index (libgpu addPrim). */
void world_menu_add_tile_primitive(RECT* rect, u8* rgb, u8 semi_trans, s32 ot_index) {
    TILE* tile = &g_world_gfx_active_packet_buffer->tiles_24[g_world_gfx_tiles_24_count++];

    tile->r0 = rgb[0];
    tile->g0 = rgb[1];
    tile->b0 = rgb[2];
    SetSemiTrans(tile, semi_trans);
    tile->x0 = rect->x + 0x80;
    tile->y0 = rect->y;
    tile->w = rect->w + 0x80;
    tile->h = rect->h;
    setaddr(tile, getaddr(&g_world_gfx_active_packet_buffer->otag[ot_index]));
    setaddr(&g_world_gfx_active_packet_buffer->otag[ot_index], tile);
}
