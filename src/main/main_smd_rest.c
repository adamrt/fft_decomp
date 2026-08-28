#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0x80 "Rest". Note flag bit 10 (0x400) and note_flags2
 * bit 1 mark the pending rest for the note dispatcher. */
u8* main_smd_rest(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    channel->rest_length = note_data[0];
    channel->active |= 0x400;
    channel->note_flags2 |= 2;
    return note_data + 1;
}
