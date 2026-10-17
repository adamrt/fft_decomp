#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xE0 "Dynamic": absolute channel volume. It ends any volume
 * ramp. */
u8* suzuki_smd_dynamic(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->volume = note_data[0] << 24;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
    channel->ramp_flags &= ~SUZUKI_RAMP_VOLUME;
    return note_data + 1;
}
