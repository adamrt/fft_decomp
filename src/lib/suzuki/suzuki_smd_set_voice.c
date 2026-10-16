#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xAA "Set Voice": assigns the channel a fixed SPU voice. A
 * voice number below 25 sets voice_mask to 1 << voice and marks every voice
 * attribute for update; a larger number clears both. */
u8* suzuki_smd_set_voice(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u8 voice = *note_data++;

    channel->voice = voice;
    if (voice < 25) {
        channel->voice_attr.voice = SPU_VOICECH(channel->voice);
        channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_ALL;
    } else {
        channel->voice_attr.voice = 0;
        channel->voice_attr_flags = 0;
    }
    return note_data;
}
