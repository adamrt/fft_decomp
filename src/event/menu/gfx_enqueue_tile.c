/* Appending a flat-colored menu rectangle to an ordering-table entry. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Takes the next TILE packet from the pool, fills it from the rectangle
 * (x offset by 0x80) and the RGB triple and links it into ordering-table
 * entry idx. */
void gfx_enqueue_tile(const RECT* rect, const rgb24_t* color, u8 semi, s32 idx) {
    TILE* tile = &g_menu_gfx_context->tiles[g_gfx_tile_count++];

    setRGB0(tile, color->r, color->g, color->b);
    SetSemiTrans(tile, semi);
    tile->x0 = rect->x + 0x80;
    tile->y0 = rect->y;
    tile->w = rect->w;
    tile->h = rect->h;
    addPrim(&g_menu_gfx_context->otag[idx], tile);
}
