#include "fft/event_card.h"
#include "psx/gpu.h"
#include "psx/types.h"

void card_gfx_init_primitive_lists(card_primitive_lists_t* lists) {
    s32 i;

    for (i = 0; i < g_card_gfx_poly_ft4_capacity; i++) {
        SetPolyFT4(&lists->polygons[i]);
        SetShadeTex(&lists->polygons[i], 0);
    }
    for (i = 0; i < g_card_gfx_tile_capacity; i++) {
        SetTile(&lists->tiles[i]);
    }
}
