#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xC1 "Envelope Shape": attack, sustain and release
 * modes. */
u8* main_smd_envelope_shape(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u32 release_mode;

    channel->attack_mode = note_data[0];
    channel->sustain_mode = note_data[1];
    release_mode = note_data[2];
    channel->func_flags |= 0x1f0;
    channel->release_mode = release_mode;
    return note_data + 3;
}
