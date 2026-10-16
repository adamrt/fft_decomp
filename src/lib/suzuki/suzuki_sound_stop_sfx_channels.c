#include "psx/suzuki.h"
#include "psx/types.h"

/* Stops every SFX channel playing sound_id and releases their voices. */
void suzuki_sound_stop_sfx_channels(s32 sound_id) {
    suzuki_music_t* music;
    suzuki_music_channel_t* channel;
    u32 bit = 1;
    u32 channels;
    u32 voices = 0;
    s32 i;

    channels = 0;
    music = g_suzuki_sound_sfx_music;
    channel = music->channels;
    for (i = SUZUKI_SFX_CHANNEL_COUNT; i != 0; i--) {
        if (channel->flags & SUZUKI_CHANNEL_IN_USE) {
            if (channel->sound_id == sound_id) {
                channels |= bit;
                voices |= channel->voice_attr.voice;
                channel->flags = 0;
            }
        }
        bit <<= 1;
        channel++;
    }
    if (channels != 0) {
        suzuki_sound_release_sfx_voices(music, channels, voices);
    }
}
