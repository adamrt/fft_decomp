#include "fft/bunit.h"
#include "psx/types.h"

void bunit_gfx_enqueue_translucent_tile(RECT* rect, u8* color, u8 semi, s32 idx) {
    TILE* tile;

    tile = &g_bunit_gfx_context->tiles[g_bunit_gfx_tile_count++];
    tile->r0 = color[0];
    tile->g0 = color[1];
    tile->b0 = color[2];
    SetSemiTrans(tile, semi);
    tile->x0 = rect->x + 0x80;
    tile->y0 = rect->y;
    tile->w = rect->w;
    tile->h = rect->h;
    {
        bunit_gfx_context_t* ctx = g_bunit_gfx_context;
        setaddr(tile, getaddr(&ctx->otag[idx]));
        setaddr(&ctx->otag[idx], tile);
    }
}
