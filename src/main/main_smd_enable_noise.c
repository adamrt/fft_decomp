#include "fft/main_sound.h"
#include "psx/spu.h"
#include "psx/types.h"

/* SMD opcode 0xB4 "Enable Noise": sets the MUS noise clock,
 * programs it with SpuSetNoiseClock and adds the channel's voice to the
 * noise voices. */
u8* main_smd_enable_noise(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->noise_clock = *note_data++;
    SpuSetNoiseClock(music->noise_clock);
    music->noise_voice_mask |= channel->voice_mask;
    channel->func_flags |= 0x10;
    return note_data;
}
