#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xDB: deactivates modulator 0. */
u8* main_smd_pitch_shift_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[0].flags &= ~1;
    return note_data;
}
