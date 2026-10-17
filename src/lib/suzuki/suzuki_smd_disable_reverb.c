#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xBB "Disable Reverb". */
u8* suzuki_smd_disable_reverb(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->reverb_voice_mask &= ~channel->voice_attr.voice;
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_SUSTAIN;
    return note_data;
}
