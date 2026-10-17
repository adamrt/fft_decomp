#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xB1 "Slur Off". */
u8* suzuki_smd_slur_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->flags &= ~SUZUKI_CHANNEL_SLUR;
    return note_data;
}
