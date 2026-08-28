#include "fft/event.h"
#include "fft/world.h"
#include "psx/types.h"

void world_script_apply_relative_camera(u8* source, const s32* deltas) {
    u8* destination;
    s32 index;
    s32 delta;
    s32 value;

    destination = source + 15;
    if (source[14] == 0x38) {
        destination = source + 18;
    }

    for (index = 0; index < 7; index++) {
        delta = *deltas;
        if (index < 3) {
            delta = delta / 1024;
        }
        if (world_script_load_halfword(source) == BATTLE_CAMERA_FUSION_HOLD_SENTINEL) {
            value = BATTLE_CAMERA_FUSION_HOLD_SENTINEL;
        } else {
            value = delta + world_script_load_halfword(source);
        }
        /* The target sign-extends the value; the definition's u16 parameter would zero-extend it. */
        ((void (*)(u8*, s16))world_script_store_halfword)(destination, value);
        source += 2;
        destination += 2;
        deltas++;
    }
}
