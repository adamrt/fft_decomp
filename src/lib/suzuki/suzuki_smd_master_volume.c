#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xA6 "Master Volume": sets the MUS master volume and marks
 * every channel for a volume update. */
u8* suzuki_smd_master_volume(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->master_volume.value = *note_data++ << 24;
    suzuki_smd_set_voice_update_flags_all_channels(SUZUKI_VOICE_UPDATE_VOLUME, music);
    return note_data;
}
