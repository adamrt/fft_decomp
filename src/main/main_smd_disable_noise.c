#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xB7 "Disable Noise". */
u8* main_smd_disable_noise(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->noise_voice_mask &= ~channel->voice_mask;
    channel->func_flags |= 0x10;
    return note_data;
}
