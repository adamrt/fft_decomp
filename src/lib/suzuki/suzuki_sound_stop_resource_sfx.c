#include "fft/suzuki.h"
#include "psx/types.h"

/* Stops every SFX channel playing a sound of the given sound resource
 * (sound id high half == resource id). suzuki_sound_remove_resource calls
 * it before unlinking the resource. */
void suzuki_sound_stop_resource_sfx(suzuki_sound_resource_t* resource) {
    suzuki_music_channel_t* channel = g_suzuki_sound_sfx_music->channels;
    s32 sound_id;
    s16 id;
    s32 i;

    id = resource->id;
    for (i = SUZUKI_SFX_CHANNEL_COUNT; i != 0; i--) {
        if (channel->flags & SUZUKI_CHANNEL_IN_USE) {
            sound_id = channel->sound_id;
            if ((sound_id >> 16) == id) {
                suzuki_sound_stop_sfx_channels(sound_id);
            }
        }
        channel++;
    }
}
