#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xCA "Release Mode". */
u8* main_smd_release_mode(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    u32 mode;

    mode = note_data[0];
    channel->func_flags |= 0x80;
    channel->release_mode = mode;
    return note_data + 1;
}
