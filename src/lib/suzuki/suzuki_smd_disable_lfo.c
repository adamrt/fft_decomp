#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xB3 "Disable LFO". */
u8* suzuki_smd_disable_lfo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->lfo_voice_mask &= ~channel->voice_attr.voice;
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_PITCH;
    return note_data;
}
