#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xF7 "Modulator Disable": deactivates the modulator selected
 * by the parameter byte. */
u8* suzuki_smd_modulator_disable(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[*note_data].flags &= ~SUZUKI_MODULATOR_ACTIVE;
    return note_data + 1;
}
