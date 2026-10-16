#include "psx/suzuki.h"
#include "psx/types.h"

/* Sets the balance of every SFX channel playing sound_id. No caller is
 * known. */
void suzuki_sound_set_sfx_balance(s32 sound_id, s32 balance) {
    suzuki_music_channel_t* channel;
    s32 count = SUZUKI_SFX_CHANNEL_COUNT;

    channel = g_suzuki_sound_sfx_music->channels;
    do {
        if (channel->flags & SUZUKI_CHANNEL_IN_USE) {
            if (channel->sound_id == sound_id) {
                channel->balance = balance << 8;
                channel->voice_update_flags = SUZUKI_VOICE_UPDATE_VOLUME;
            }
        }
        channel++;
    } while (--count != 0);
}
