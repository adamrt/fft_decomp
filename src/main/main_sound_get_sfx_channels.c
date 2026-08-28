#include "fft/main_sound.h"
#include "psx/types.h"

/* Returns the mask of SFX channels playing sound_id, or of every active SFX
 * channel when sound_id is -1. */
u32 main_sound_get_sfx_channels(s32 sound_id) {
    suzuki_music_channel_t* channel;
    u32 bit;
    u32 channels;
    s32 count;

    count = 8;
    bit = 1;
    channel = g_main_sound_sfx_music->channels;
    channels = 0;
    if (sound_id == -1) {
        do {
            if (channel->active & 1) {
                channels |= bit;
            }
            channel++;
            bit <<= 1;
        } while (--count != 0);
    } else {
        do {
            if (channel->active & 1) {
                if (channel->sound_id.raw == sound_id) {
                    channels |= bit;
                }
            }
            channel++;
            bit <<= 1;
        } while (--count != 0);
    }
    return channels;
}
