#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xE7 "Tremolo Off": deactivates modulator 1. */
u8* suzuki_smd_tremolo_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[SUZUKI_MODULATOR_SLOT_TREMOLO].flags &= ~SUZUKI_MODULATOR_ACTIVE;
    return note_data;
}
