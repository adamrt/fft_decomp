#include "fft/card.h"
#include "psx/types.h"

typedef struct card_tile {
    u32 tag;
    u8 r;
    u8 g;
    u8 b;
    s16 x;
    s16 y;
    s16 w;
    s16 h;
} card_tile_t;

void card_gfx_enqueue_translucent_tile(const RECT* rect, const u8* color, s32 semitrans, s32 otag_index) {
    card_tile_t* tile;
    const RECT* tile_rect = rect;
    u16 index = g_card_gfx_tile_count;

    g_card_gfx_tile_count = index + 1;
    tile = &((card_tile_t*)g_card_gfx_context->tiles)[index];
    tile->r = color[0];
    tile->g = color[1];
    tile->b = color[2];
    SetSemiTrans(tile, semitrans & 0xff);
    tile->x = tile_rect->x + 0x80;
    tile->y = tile_rect->y;
    tile->w = tile_rect->w;
    tile->h = tile_rect->h;
    {
        card_graphics_context_t* context = g_card_gfx_context;
        setaddr(tile, getaddr(&context->otag[otag_index]));
        setaddr(&context->otag[otag_index], tile);
    }
}
