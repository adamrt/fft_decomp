#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xD6 "Portamento": the ticks each key-on takes to slide from
 * the previous key (suzuki_smd_read_instructions); 0 turns it off. */
u8* suzuki_smd_portamento(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 value = *note_data++;

    channel->portamento_ticks = value;
    if (value != 0)
        channel->ramp_flags |= SUZUKI_RAMP_PORTAMENTO;
    else
        channel->ramp_flags &= ~SUZUKI_RAMP_PORTAMENTO;
    return note_data;
}
