#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xD1: add to the channel pitch offset in
 * 1/8 semitone units (32 of 256). Coarse twin of suzuki_smd_add_pitch_offset_fine. */
u8* suzuki_smd_add_pitch_offset_coarse(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->pitch_offset += (s8)note_data[0] << 5;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_PITCH;
    return note_data + 1;
}
