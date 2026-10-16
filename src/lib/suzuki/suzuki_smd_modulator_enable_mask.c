#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xF5 "Modulator Enable Mask": bit n of the parameter restarts
 * and activates modulator n, a clear bit deactivates it (modulators 0-3). */
u8* suzuki_smd_modulator_enable_mask(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_modulator_t* modulator = channel->modulators;
    s32 i = SUZUKI_MODULATOR_COUNT;
    u8 mask = *note_data++;

    do {
        if (mask & 1) {
            suzuki_smd_modulator_reset(modulator);
            modulator->flags |= SUZUKI_MODULATOR_ACTIVE;
        } else {
            modulator->flags &= ~SUZUKI_MODULATOR_ACTIVE;
        }
        mask >>= 1;
        modulator++;
    } while (--i);
    return note_data;
}
