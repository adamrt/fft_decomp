#include "fft/main.h"
#include "psx/types.h"

/* Points each channel's restart stream at its SMD channel data (a zero
 * offset or a channel past the header's count gets no stream) and marks the
 * stream set (active bit 0x4000). */
void main_smd_init_channel_streams(suzuki_music_t* music, suzuki_smd_header_t* smd) {
    suzuki_music_channel_t* channel;
    u16* offsets;
    s32 remaining;
    s32 count;

    channel = music->channels;
    offsets = smd->channel_offsets;
    count = music->channel_count;
    remaining = smd->channel_count;
    do {
        channel->active |= 0x4000;
        if (remaining != 0) {
            channel->restart_note_data = *offsets != 0 ? (u8*)smd + *offsets : 0;
            remaining--;
            offsets++;
        } else {
            channel->restart_note_data = 0;
        }
        channel++;
    } while (--count != 0);
}
