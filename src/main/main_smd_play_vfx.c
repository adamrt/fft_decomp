#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0x9E "Play VFX SMD": moves the channel into a loaded
 * VFX sound resource. The resource is the one whose id matches the high half
 * of the channel's sound id (id 0 takes the list head). Its channel_offsets
 * entry sound * 2 + byte 2, with the sound number little-endian in bytes 0-1,
 * gives the stream, and reading resumes three bytes past it. Without a matching resource
 * the read position is returned unchanged, so the parameter bytes are read
 * next as note data. */
u8* main_smd_play_vfx(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    main_sound_resource_t* resource;
    s16 sound;
    s16 id;
    u8 index;

    resource = g_main_sound_resource_list;
    sound = note_data[0] | (note_data[1] << 8);
    id = channel->sound_id.halves.high;
    index = note_data[2];
    if (id != 0) {
        while (resource->id != id) {
            resource = resource->next;
            if (resource == 0) {
                return note_data;
            }
        }
    }
    note_data = (u8*)resource + resource->channel_offsets[sound * 2 + index];
    return note_data + 3;
}
