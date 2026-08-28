#include "fft/main_sound.h"
#include "psx/api.h"
#include "psx/etc.h"
#include "psx/types.h"

/* Silences the eight channels of the SFX MUS record and queues their voices
 * for key-off. It leaves the music records alone. */
void SuzukiTurnOffAllMusic(void) {
    suzuki_music_t* music;
    suzuki_music_channel_t* channel;
    u32 voices;
    s32 i;

    voices = 0;
    music = g_main_sound_sfx_music;
    channel = music->channels;
    for (i = 8; i != 0; i--) {
        if (channel->active & 1) {
            channel->active = 0;
            voices |= channel->voice_mask;
        }
        channel++;
    }
    DisableEvent(g_main_root_counter_2_event);
    music->channel_mask = 0;
    music->key_on_mask = 0;
    music->lfo_voice_mask = 0;
    music->noise_voice_mask = 0;
    music->reverb_voice_mask = 0;
    g_main_mask_exclusion = 0;
    g_main_sound_sfx_key_off_voices |= voices;
    EnableEvent(g_main_root_counter_2_event);
}
