#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0x80 "Rest": keys the channel off and waits the given number
 * of ticks. */
u8* suzuki_smd_rest(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->event_ticks = note_data[0];
    channel->flags |= SUZUKI_CHANNEL_KEYED_OFF;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_KEY_OFF;
    return note_data + 1;
}
