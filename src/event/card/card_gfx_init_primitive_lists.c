#include "fft/event_card.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Initialize the POLY_FT4 and TILE primitive headers of one graphics context. */
void card_gfx_init_primitive_lists(card_graphics_context_t* lists) {
    s32 i;

    for (i = 0; i < g_card_gfx_poly_ft4_capacity; i++) {
        SetPolyFT4(&lists->polygons[i]);
        SetShadeTex(&lists->polygons[i], 0);
    }
    for (i = 0; i < g_card_gfx_tile_capacity; i++) {
        SetTile(&lists->tiles[i]);
    }
}
