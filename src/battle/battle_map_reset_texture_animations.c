#include "fft/battle.h"
#include "psx/types.h"

void battle_map_reset_texture_animations(void) {
    s32 offset;

    offset = (MAP_TEXTURE_ANIMATION_CAPACITY - 1) * MAP_TEXTURE_ANIMATION_STATE_BYTES;
    do {
        ((u8*)g_battle_map_texture_animation_states)[offset] = 0;
        offset -= MAP_TEXTURE_ANIMATION_STATE_BYTES;
    } while (offset >= 0);
}
