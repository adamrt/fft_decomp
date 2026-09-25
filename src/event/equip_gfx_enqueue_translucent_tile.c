#include "fft/event_equip.h"
#include "psx/types.h"

void equip_gfx_enqueue_translucent_tile(const RECT* rect, const u8* color, s32 semitrans, s32 otag_index) {
    TILE* tile;
    const RECT* tile_rect = rect;
    u16 index = g_equip_gfx_tile_count;

    g_equip_gfx_tile_count = index + 1;
    tile = &g_equip_gfx_context->tiles[index];
    tile->r0 = color[0];
    tile->g0 = color[1];
    tile->b0 = color[2];
    SetSemiTrans(tile, semitrans & 0xff);
    tile->x0 = tile_rect->x + 0x80;
    tile->y0 = tile_rect->y;
    tile->w = tile_rect->w;
    tile->h = tile_rect->h;
    {
        equip_gfx_context_t* context = g_equip_gfx_context;
        setaddr(tile, getaddr(&context->otag[otag_index]));
        setaddr(&context->otag[otag_index], tile);
    }
}
