#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0x9E "Play VFX": moves the channel into a loaded sound
 * resource. The resource is the one whose id matches the high half
 * of the channel's sound id (id 0 takes the list head). Its channel_offsets
 * entry sound * 2 + byte 2, with the sound number little-endian in bytes 0-1,
 * gives the stream, and reading resumes three bytes past it. Without a matching resource
 * the read position is returned unchanged, so the parameter bytes are read
 * next as note data. */
u8* suzuki_smd_play_vfx(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_sound_resource_t* resource = g_suzuki_sound_resource_list;
    s16 sound = note_data[0] | (note_data[1] << 8);
    s16 id = channel->sound_id >> 16;
    u8 index = note_data[2];

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
