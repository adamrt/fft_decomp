#include "fft/suzuki.h"
#include "psx/types.h"

/* Marks every in-use channel that is sounding and is not muted or flagged
 * SUZUKI_CHANNEL_NO_RETRIGGER for a new key-on and returns the voices of
 * those channels.
 *
 * The two single-bit tests are merged by GCC into one 32-bit
 * `(flags & 0x101) == 0x101` test (`lw`), which the target contains; a
 * direct 0x101 mask test compiles to `lhu`. */
u32 suzuki_smd_retrigger_held_voices(suzuki_music_t* music) {
    suzuki_music_channel_t* channel;
    s32 count = music->channel_count;
    u32 voices;

    channel = music->channels;
    voices = 0;
    do {
        if ((channel->flags & SUZUKI_CHANNEL_IN_USE) && (channel->flags & SUZUKI_CHANNEL_SOUNDING)) {
            if ((channel->flags & (SUZUKI_CHANNEL_MUTED | SUZUKI_CHANNEL_NO_RETRIGGER)) == 0) {
                voices |= channel->voice_attr.voice;
                channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_KEY_ON;
            }
        }
        count--;
        channel++;
    } while (count != 0);
    return voices;
}
