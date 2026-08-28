#include "fft/main_sound.h"
#include "psx/types.h"

/* Mutes the channels whose bit is set in mask (bit n = channel n) and
 * unmutes the others. A newly muted channel's voice is queued for key-off;
 * a newly unmuted one is queued for key-on when active has 0x100 set and
 * 0x10 clear. GCC merges those two bit tests into one word-wide `lw`/`andi`
 * test, as in the target. */
void main_smd_set_channel_mute_mask(suzuki_music_t* music, u32 mask) {
    suzuki_music_channel_t* channel;
    u16 key_off;
    u16 key_on;
    s32 count;

    if (music != 0) {
        key_off = 0;
        key_on = 0;
        channel = music->channels;
        count = music->channel_count;
        music->mute_mask = mask;
        do {
            if (channel->active != 0) {
                if (mask & 1) {
                    if (!(channel->active & 0x20)) {
                        channel->active |= 0x20;
                        key_off |= channel->voice_mask;
                    }
                } else {
                    if (channel->active & 0x20) {
                        channel->active &= ~0x20;
                        if ((channel->active & 0x100) && !(channel->active & 0x10)) {
                            key_on |= channel->voice_mask;
                        }
                    }
                }
            }
            channel++;
            mask >>= 1;
        } while (--count != 0);
        music->key_on_mask |= key_on;
        music->key_off_mask |= key_off;
    }
}
