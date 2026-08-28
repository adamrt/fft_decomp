#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xB3 "Disable LFO". */
u8* main_smd_disable_lfo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->lfo_voice_mask &= ~channel->voice_mask;
    channel->func_flags |= 4;
    return note_data;
}
