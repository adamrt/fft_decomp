#include "psx/suzuki.h"

/* Allocates a MUS record for an SMD sequence, reads its header and channels,
 * and queues it. The returned record is the caller's music handle. */
suzuki_music_t* suzuki_smd_create_music(suzuki_smd_header_t* smd) {
    suzuki_music_t* music
        = suzuki_sound_alloc(smd->channel_count * sizeof(suzuki_music_channel_t) + sizeof(suzuki_music_t));

    music->smd = smd;
    suzuki_smd_transfer_music_data(music);
    suzuki_smd_init_channels(music);
    music->mute_mask = 0;
    suzuki_smd_insert_music(music);
    return music;
}
