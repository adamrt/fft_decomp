#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xE1 "Crescendo": signed volume delta. Like "Dynamic"
 * (0xE0) it marks a volume change and ends any volume ramp. */
u8* main_smd_crescendo(s8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s32 volume_delta;

    volume_delta = note_data[0];
    channel->volume = (channel->volume + (volume_delta << 24)) & 0x7fffffff;
    channel->note_flags2 |= 0x100;
    channel->flags_06 &= ~8;
    return note_data + 1;
}
