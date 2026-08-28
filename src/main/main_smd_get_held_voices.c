#include "fft/main_sound.h"
#include "psx/types.h"

/* Returns the voices of every in-use channel that holds a fermata and is not
 * muted or flagged 0x10: the read-only twin of
 * main_smd_retrigger_held_voices, with the same merged bit test. */
u32 main_smd_get_held_voices(suzuki_music_t* music) {
    suzuki_music_channel_t* channel;
    s32 count;
    u32 voices;

    count = music->channel_count;
    channel = music->channels;
    voices = 0;
    do {
        if ((channel->active & 1) && (channel->active & 0x100)) {
            if ((channel->active & 0x30) == 0) {
                voices |= channel->voice_mask;
            }
        }
        count--;
        channel++;
    } while (count != 0);
    return voices;
}
