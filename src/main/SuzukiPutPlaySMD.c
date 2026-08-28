#include "fft/main_sound.h"
#include "psx/types.h"

/* Allocates a MUS record for an SMD sequence, reads its header and channels,
 * and queues it. The returned record is the caller's music handle. */
suzuki_music_t* SuzukiPutPlaySMD(suzuki_smd_header_t* smd) {
    suzuki_music_t* music;

    music = main_sound_alloc(smd->channel_count * sizeof(suzuki_music_channel_t) + 0xb8);
    music->smd = smd;
    main_smd_transfer_music_data(music);
    main_smd_init_channels(music);
    music->mute_mask = 0;
    main_smd_insert_music(music);
    return music;
}
