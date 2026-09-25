#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xBB "Disable Reverb". */
u8* main_smd_disable_reverb(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->reverb_voice_mask &= ~channel->voice_mask;
    channel->func_flags |= 0x40;
    return note_data;
}
