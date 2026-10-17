#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xEE "Pan LFO On": activates modulator 2. */
u8* suzuki_smd_pan_lfo_on(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[SUZUKI_MODULATOR_SLOT_PAN_LFO].flags |= SUZUKI_MODULATOR_ACTIVE;
    return note_data;
}
