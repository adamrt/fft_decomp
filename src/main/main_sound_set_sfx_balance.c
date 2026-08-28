#include "fft/main_sound.h"
#include "psx/types.h"

/* Sets the balance of every SFX channel playing sound_id. No caller is
 * known. */
void main_sound_set_sfx_balance(s32 sound_id, s32 balance) {
    suzuki_music_channel_t* channel;
    s32 count;

    count = 8;
    channel = g_main_sound_sfx_music->channels;
    do {
        if (channel->active & 1) {
            if (channel->sound_id.raw == sound_id) {
                channel->balance = balance << 8;
                channel->note_flags2 = 0x100;
            }
        }
        channel++;
    } while (--count != 0);
}
