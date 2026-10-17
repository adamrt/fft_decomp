#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xB2 "Enable LFO": adds the channel's voice to the SPU
 * pitch-LFO voices, if its voice number is odd. */
u8* suzuki_smd_enable_lfo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    if ((channel->voice & 1) != 0) {
        music->lfo_voice_mask |= channel->voice_attr.voice;
        channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_PITCH;
    }
    return note_data;
}
