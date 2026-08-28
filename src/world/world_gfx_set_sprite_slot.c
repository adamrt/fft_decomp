#include "fft/world.h"

void world_gfx_set_sprite_slot(s32 slot_index, s32 x, s32 y, u16* tile_table) {
    g_world_gfx_sprite_slots[slot_index].x = x;
    g_world_gfx_sprite_slots[slot_index].y = y;
    g_world_gfx_sprite_slots[slot_index].tile = tile_table[x] & 0x3FF;
}
