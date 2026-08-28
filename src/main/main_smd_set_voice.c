#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xAA: assigns the channel a fixed SPU
 * voice. A voice number below 25 sets voice_mask to 1 << voice and marks
 * every voice parameter for update (func_flags 0x1ff); a larger number
 * clears both. */
u8* main_smd_set_voice(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    u8 voice;

    voice = *note_data++;
    channel->voice = voice;
    if (voice < 25) {
        channel->voice_mask = 1 << channel->voice;
        channel->func_flags |= 0x1ff;
    } else {
        channel->voice_mask = 0;
        channel->func_flags = 0;
    }
    return note_data;
}
