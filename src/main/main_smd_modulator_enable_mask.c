#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xF5: bit n of the parameter restarts and
 * activates modulator n, a clear bit deactivates it (modulators 0-3). */
u8* main_smd_modulator_enable_mask(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    suzuki_modulator_t* modulator;
    s32 i;
    u32 mask;

    modulator = channel->modulators;
    i = 4;
    mask = *note_data++;
    do {
        if (mask & 1) {
            main_smd_modulator_reset(modulator);
            modulator->flags |= 1;
        } else {
            modulator->flags &= ~1;
        }
        mask >>= 1;
        modulator++;
    } while (--i);
    return note_data;
}
