#include "fft/world.h"
#include "psx/types.h"

void world_gfx_get_sprite_slot(s32 slot_index, s16* x, s16* y, s16* tile_table) {
    s32 tile;
    s32 found_end;
    s32 i;

    *x = g_world_gfx_sprite_slots[slot_index].x;
    *y = g_world_gfx_sprite_slots[slot_index].y;
    tile = g_world_gfx_sprite_slots[slot_index].tile;
    found_end = 0;
    for (i = 0; i < *x; i++) {
        if (tile_table[i] == -1) {
            found_end = 1;
        }
    }
    if (tile == tile_table[*x] && found_end == 0) {
        return;
    }
    for (i = 0; tile_table[i] != -1; i++) {
        if ((tile_table[i] & 0x3ff) == tile) {
            *x = i;
            return;
        }
    }
    *x = 0;
    *y = 0;
}
