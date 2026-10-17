#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xEF "Pan LFO Off": deactivates modulator 2. */
u8* suzuki_smd_pan_lfo_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[SUZUKI_MODULATOR_SLOT_PAN_LFO].flags &= ~SUZUKI_MODULATOR_ACTIVE;
    return note_data;
}
