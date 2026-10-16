#include "psx/suzuki.h"
#include "psx/types.h"

/* ORs a mask of suzuki_channel_flag_e bits into the flags of every channel
 * that is in use. No caller is known. */
void suzuki_smd_set_flags_all_channels(u16 mask, suzuki_music_t* music) {
    suzuki_music_channel_t* channel = music->channels;
    s32 count = music->channel_count;

    do {
        if (channel->flags != 0)
            channel->flags |= mask;
        channel++;
    } while (--count != 0);
}
