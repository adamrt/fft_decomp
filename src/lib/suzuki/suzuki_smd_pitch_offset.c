#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xD0 "Pitch Offset": sets the channel pitch offset from a
 * signed byte in 1/8 semitone units. */
u8* suzuki_smd_pitch_offset(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->pitch_offset = (s8)note_data[0] << 5;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_PITCH;
    return note_data + 1;
}
