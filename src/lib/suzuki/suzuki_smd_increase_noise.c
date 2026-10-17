#include "fft/suzuki.h"
#include "psx/libspu.h"
#include "psx/types.h"

/* SMD opcode 0xB5 "Increase Noise": adds its byte to the MUS
 * noise clock (modulo 64), then acts like Enable Noise (0xB4). */
u8* suzuki_smd_increase_noise(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->noise_clock = (*note_data++ + music->noise_clock) & 0x3f;
    SpuSetNoiseClock(music->noise_clock);
    music->noise_voice_mask |= channel->voice_attr.voice;
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_ATTACK;
    return note_data;
}
