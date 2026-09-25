#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xC2 "Attack Time". */
u8* main_smd_attack_time(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 value;

    value = *note_data++;
    channel->func_flags |= 0x10;
    channel->attack_time = value;
    return note_data;
}
