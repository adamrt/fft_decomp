#include "fft/main_sound.h"
#include "psx/types.h"

/* ORs a mask into the note flag word of every channel that already has one. */
void main_smd_set_flags_all_channels(u16 mask, suzuki_music_t* music) {
    suzuki_music_channel_t* channel;
    s32 count;

    channel = music->channels;
    count = music->channel_count;
    do {
        if (channel->active != 0)
            channel->active |= mask;
        channel++;
    } while (--count != 0);
}
