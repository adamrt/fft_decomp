#include "fft/main.h"
#include "psx/types.h"

/* Stops every SFX channel playing a sound of the given VFX resource (sound
 * id high half == resource id). main_sound_remove_vfx_resource calls it
 * before unlinking the resource. */
void main_sound_stop_resource_sfx(main_sound_resource_t* resource) {
    suzuki_music_channel_t* channel;
    s32 sound_id;
    s16 id;
    s32 i;

    channel = g_main_sound_sfx_music->channels;
    id = resource->id;
    for (i = 8; i != 0; i--) {
        if (channel->active & 1) {
            sound_id = channel->sound_id.raw;
            if ((sound_id >> 16) == id) {
                main_sound_stop_sfx_channels(sound_id);
            }
        }
        channel++;
    }
}
