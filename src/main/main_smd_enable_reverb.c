#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xBA "Enable Reverb". */
u8* main_smd_enable_reverb(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->reverb_voice_mask |= channel->voice_mask;
    channel->func_flags |= 0x40;
    return note_data;
}
