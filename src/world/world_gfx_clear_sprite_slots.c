#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

void world_gfx_clear_sprite_slots(void) {
    memset(g_world_gfx_sprite_slots, 0, 0x54);
}
