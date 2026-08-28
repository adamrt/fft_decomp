#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xEF: deactivates modulator 2. */
u8* main_smd_pan_lfo_off(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    channel->modulators[2].flags &= ~1;
    return note_data;
}
