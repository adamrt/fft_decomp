#include "fft/suzuki.h"
#include "psx/types.h"

/* Allocates the SFX music record (0xbb8 bytes: the header and eight
 * channels), assigns channel n to SPU voice 16 + n and queues the record. */
suzuki_music_t* suzuki_sound_init_sfx_music(void) {
    suzuki_music_t* music
        = suzuki_sound_alloc(sizeof(suzuki_music_t) + SUZUKI_SFX_CHANNEL_COUNT * sizeof(suzuki_music_channel_t));
    suzuki_music_channel_t* channel;
    s32 voice;
    s32 index;
    s32 count;

    suzuki_smd_init_sfx_music_header(music);
    channel = music->channels;
    voice = SUZUKI_SFX_VOICE_FIRST;
    count = SUZUKI_SFX_CHANNEL_COUNT;
    index = 0;
    do {
        channel->flags = 0;
        channel->channel_number = index++;
        channel->voice = voice;
        channel->voice_attr.voice = SPU_VOICECH(voice);
        channel++;
        voice++;
    } while (--count != 0);
    suzuki_smd_insert_music(music);
    return music;
}
