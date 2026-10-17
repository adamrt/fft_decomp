#include "fft/suzuki.h"
#include "psx/types.h"

/* Sets the velocity of every SFX channel playing sound_id, or stops those
 * channels when volume is 0.
 *
 * The stop branch repeats suzuki_sound_stop_sfx_channels with its own locals
 * and its own read of g_suzuki_sound_sfx_music, which is what the target's
 * register allocation reflects. */
void suzuki_sound_set_sfx_volume(int sound_id, s16 volume) {
    suzuki_music_t* music;
    suzuki_music_channel_t* channel;
    s32 count = SUZUKI_SFX_CHANNEL_COUNT;

    music = g_suzuki_sound_sfx_music;
    channel = music->channels;
    if (volume != 0) {
        do {
            if (channel->flags & SUZUKI_CHANNEL_IN_USE) {
                if (channel->sound_id == sound_id) {
                    channel->velocity = volume << 8;
                    channel->voice_update_flags = SUZUKI_VOICE_UPDATE_VOLUME;
                }
            }
            channel++;
        } while (--count != 0);
    } else {
        suzuki_music_t* sfx;
        suzuki_music_channel_t* stop;
        u32 bit;
        u32 channels;
        u32 voices;
        s32 remaining;

        sfx = g_suzuki_sound_sfx_music;
        stop = sfx->channels;
        bit = 1;
        voices = 0;
        channels = 0;
        remaining = SUZUKI_SFX_CHANNEL_COUNT;
        do {
            if (stop->flags & SUZUKI_CHANNEL_IN_USE) {
                if (stop->sound_id == sound_id) {
                    channels |= bit;
                    voices |= stop->voice_attr.voice;
                    stop->flags = 0;
                }
            }
            bit <<= 1;
            stop++;
        } while (--remaining != 0);
        if (channels != 0) {
            suzuki_sound_release_sfx_voices(sfx, channels, voices);
        }
    }
}
