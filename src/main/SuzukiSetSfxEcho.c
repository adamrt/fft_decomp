#include "fft/main_sound.h"
#include "psx/types.h"

/* Sets the velocity of every SFX channel playing sound_id, or stops those
 * channels when echo is 0.
 *
 * The stop branch repeats main_sound_stop_sfx_channels with its own locals
 * and its own read of g_main_sound_sfx_music, which is what the target's
 * register allocation reflects. */
void SuzukiSetSfxEcho(int sound_id, s16 echo) {
    suzuki_music_t* music;
    suzuki_music_channel_t* channel;
    s32 count;

    count = 8;
    music = g_main_sound_sfx_music;
    channel = music->channels;
    if (echo != 0) {
        do {
            if (channel->active & 1) {
                if (channel->sound_id.raw == sound_id) {
                    channel->velocity = echo << 8;
                    channel->note_flags2 = 0x100;
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

        sfx = g_main_sound_sfx_music;
        stop = sfx->channels;
        bit = 1;
        voices = 0;
        channels = 0;
        remaining = 8;
        do {
            if (stop->active & 1) {
                if (stop->sound_id.raw == sound_id) {
                    channels |= bit;
                    voices |= stop->voice_mask;
                    stop->active = 0;
                }
            }
            bit <<= 1;
            stop++;
        } while (--remaining != 0);
        if (channels != 0) {
            main_sound_release_sfx_voices(sfx, channels, voices);
        }
    }
}
