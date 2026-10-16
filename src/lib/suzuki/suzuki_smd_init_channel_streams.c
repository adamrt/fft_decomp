#include "psx/suzuki.h"
#include "psx/types.h"

/* Points each channel's restart stream at its SMD channel data (a zero
 * offset or a channel past the header's count gets no stream) and marks the
 * stream set (SUZUKI_CHANNEL_STREAMS_SET). */
void suzuki_smd_init_channel_streams(suzuki_music_t* music, suzuki_smd_header_t* smd) {
    suzuki_music_channel_t* channel = music->channels;
    u16* offsets = smd->channel_offsets;
    s32 remaining;
    s32 count = music->channel_count;

    remaining = smd->channel_count;
    do {
        channel->flags |= SUZUKI_CHANNEL_STREAMS_SET;
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
