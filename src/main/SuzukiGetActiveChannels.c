#include "fft/main.h"
#include "psx/types.h"

/* Returns the SPU voices owned by the channels of music that are in use. */
u32 SuzukiGetActiveChannels(suzuki_music_t* music) {
    suzuki_music_channel_t* channel;
    s32 remaining;
    u32 active;

    remaining = music->channel_count;
    channel = music->channels;
    active = 0;
    do {
        if (channel->active != 0) {
            active |= channel->voice_mask;
        }
        remaining--;
        channel++;
    } while (remaining != 0);
    return active;
}
