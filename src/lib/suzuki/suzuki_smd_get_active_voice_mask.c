#include "psx/suzuki.h"
#include "psx/types.h"

/* Returns the SPU voices owned by the channels of music that are in use. */
u32 suzuki_smd_get_active_voice_mask(suzuki_music_t* music) {
    suzuki_music_channel_t* channel;
    s32 remaining = music->channel_count;
    u32 active;

    channel = music->channels;
    active = 0;
    do {
        if (channel->flags != 0) {
            active |= channel->voice_attr.voice;
        }
        remaining--;
        channel++;
    } while (remaining != 0);
    return active;
}
