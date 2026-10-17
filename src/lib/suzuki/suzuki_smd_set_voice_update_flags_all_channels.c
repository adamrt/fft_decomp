#include "fft/suzuki.h"
#include "psx/types.h"

/* ORs mask into voice_update_flags of every channel that is in use. */
void suzuki_smd_set_voice_update_flags_all_channels(u16 mask, suzuki_music_t* music) {
    suzuki_music_channel_t* channel = music->channels;
    s32 count = music->channel_count;

    do {
        if (channel->flags != 0)
            channel->voice_update_flags |= mask;
        channel++;
    } while (--count != 0);
}
