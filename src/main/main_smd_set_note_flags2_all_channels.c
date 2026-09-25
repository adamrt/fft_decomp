#include "fft/main.h"
#include "psx/types.h"

/* ORs mask into note_flags2 of every channel that is in use. */
void main_smd_set_note_flags2_all_channels(u16 mask, suzuki_music_t* music) {
    suzuki_music_channel_t* channel;
    s32 count;

    channel = music->channels;
    count = music->channel_count;
    do {
        if (channel->active != 0)
            channel->note_flags2 |= mask;
        channel++;
    } while (--count != 0);
}
