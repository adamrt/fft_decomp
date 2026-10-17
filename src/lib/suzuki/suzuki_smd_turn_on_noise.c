#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xB6 "Turn On Noise". */
u8* suzuki_smd_turn_on_noise(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->noise_voice_mask |= channel->voice_attr.voice;
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_ATTACK;
    return note_data;
}
