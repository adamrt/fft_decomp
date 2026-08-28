#include "fft/main_sound.h"
#include "psx/types.h"

/* Allocates the SFX music record (0xbb8 bytes: the header and eight
 * channels), assigns channel n to SPU voice 16 + n and queues the record. */
suzuki_music_t* main_sound_init_sfx_music(void) {
    suzuki_music_t* music;
    suzuki_music_channel_t* channel;
    s32 voice;
    s32 index;
    s32 count;

    music = main_sound_alloc(0xbb8);
    main_smd_init_sfx_music_header(music);
    channel = music->channels;
    voice = 16;
    count = 8;
    index = 0;
    do {
        channel->active = 0;
        channel->channel_number = index++;
        channel->voice = voice;
        channel->voice_mask = 1 << voice;
        channel++;
        voice++;
    } while (--count != 0);
    main_smd_insert_music(music);
    return music;
}
