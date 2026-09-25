#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xB2 "Enable LFO". */
u8* main_smd_enable_lfo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    if ((channel->voice & 1) != 0) {
        music->lfo_voice_mask |= channel->voice_mask;
        channel->func_flags |= 4;
    }
    return note_data;
}
