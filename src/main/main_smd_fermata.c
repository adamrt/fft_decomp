#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0x81 "Fermata": stores the hold length in the rest
 * length field and sets bit 8 of the note flag word. */
u8* main_smd_fermata(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 value;

    value = *note_data++;
    channel->active |= 0x100;
    channel->rest_length = value;
    return note_data;
}
