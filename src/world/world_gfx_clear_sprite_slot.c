#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

void world_gfx_clear_sprite_slot(s32 index) {
    memset(&g_world_gfx_sprite_slots[index], 0, sizeof(world_gfx_sprite_slot_t));
}
