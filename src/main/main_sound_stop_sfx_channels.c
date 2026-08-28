#include "fft/main_sound.h"
#include "psx/types.h"

/* Stops every SFX channel playing sound_id and releases their voices. */
void main_sound_stop_sfx_channels(s32 sound_id) {
    suzuki_music_t* music;
    suzuki_music_channel_t* channel;
    u32 bit;
    u32 channels;
    u32 voices;
    s32 i;

    bit = 1;
    voices = 0;
    channels = 0;
    music = g_main_sound_sfx_music;
    channel = music->channels;
    for (i = 8; i != 0; i--) {
        if (channel->active & 1) {
            if (channel->sound_id.raw == sound_id) {
                channels |= bit;
                voices |= channel->voice_mask;
                channel->active = 0;
            }
        }
        bit <<= 1;
        channel++;
    }
    if (channels != 0) {
        main_sound_release_sfx_voices(music, channels, voices);
    }
}
