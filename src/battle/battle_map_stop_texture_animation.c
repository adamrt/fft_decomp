#include "fft/battle.h"
#include "psx/types.h"

/* Preserve the target's unresolved one-slot displacement: valid handles are
 * 1..8, but the clear uses handle * 0x0a, not (handle - 1) * 0x0a. The start
 * routine returns slot index + 1. Keep this byte view until their apparent
 * indexing disagreement is explained; do not silently normalize the handle. */
s32 battle_map_stop_texture_animation(s32 handle) {
    if ((u32)(handle - 1) < MAP_TEXTURE_ANIMATION_CAPACITY) {
        ((u8*)g_battle_map_texture_animation_states)[handle * MAP_TEXTURE_ANIMATION_STATE_BYTES] = 0;
        return 1;
    }
    return 0;
}
