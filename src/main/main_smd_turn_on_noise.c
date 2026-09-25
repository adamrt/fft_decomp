#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xB6 "Turn On Noise". */
u8* main_smd_turn_on_noise(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->noise_voice_mask |= channel->voice_mask;
    channel->func_flags |= 0x10;
    return note_data;
}
