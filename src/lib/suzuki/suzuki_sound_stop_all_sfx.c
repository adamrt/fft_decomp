#include "fft/suzuki.h"
#include "psx/libapi.h"
#include "psx/types.h"

/* Silences the eight channels of the SFX MUS record and queues their voices
 * for key-off. It leaves the music records alone. */
void suzuki_sound_stop_all_sfx(void) {
    suzuki_music_t* music;
    suzuki_music_channel_t* channel;
    u32 voices = 0;
    s32 i;

    music = g_suzuki_sound_sfx_music;
    channel = music->channels;
    for (i = SUZUKI_SFX_CHANNEL_COUNT; i != 0; i--) {
        if (channel->flags & SUZUKI_CHANNEL_IN_USE) {
            channel->flags = 0;
            voices |= channel->voice_attr.voice;
        }
        channel++;
    }
    DisableEvent(g_suzuki_sound_root_counter_2_event);
    music->channel_mask = 0;
    music->key_on_mask = 0;
    music->lfo_voice_mask = 0;
    music->noise_voice_mask = 0;
    music->reverb_voice_mask = 0;
    g_suzuki_sound_sfx_reserved_voices = 0;
    g_suzuki_sound_sfx_key_off_voices |= voices;
    EnableEvent(g_suzuki_sound_root_counter_2_event);
}
