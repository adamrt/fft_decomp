#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0x81 "Fermata": holds the sounding note for the given number
 * of ticks without keying it on again. */
u8* suzuki_smd_fermata(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 value = *note_data++;

    channel->flags |= SUZUKI_CHANNEL_SOUNDING;
    channel->event_ticks = value;
    return note_data;
}
