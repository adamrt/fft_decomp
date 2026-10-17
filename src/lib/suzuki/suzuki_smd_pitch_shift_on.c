#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xDA "Vibrato On": activates modulator 0. */
u8* suzuki_smd_pitch_shift_on(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].flags |= SUZUKI_MODULATOR_ACTIVE;
    return note_data;
}
