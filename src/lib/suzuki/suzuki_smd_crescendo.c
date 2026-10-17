#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xE1 "Crescendo": signed volume delta. Like "Dynamic"
 * (0xE0) it marks a volume change and ends any volume ramp. */
u8* suzuki_smd_crescendo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s8 volume_delta = note_data[0];

    channel->volume = (channel->volume + (volume_delta << 24)) & 0x7fffffff;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
    channel->ramp_flags &= ~SUZUKI_RAMP_VOLUME;
    return note_data + 1;
}
