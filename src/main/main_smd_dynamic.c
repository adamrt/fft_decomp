#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xE0 "Dynamic": absolute channel volume. An absolute
 * volume ends any crescendo/decrescendo ramp (flags_06 bit 3). */
u8* main_smd_dynamic(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    channel->volume = note_data[0] << 24;
    channel->note_flags2 |= 0x100;
    channel->flags_06 &= ~8;
    return note_data + 1;
}
