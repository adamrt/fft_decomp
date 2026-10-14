#include "psx/suzuki.h"
#include "psx/types.h"

/* Plays a sound effect on a free pair of SFX voices at the default volume
 * (0x60) and center balance (0x40). */
void suzuki_sound_play_sfx(s32 sound_id) {
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SFX_ENABLED) != 0) {
        s32 channel = suzuki_sound_find_sfx_channel(sound_id, 2);
        g_suzuki_sound_sfx_channel_count = 2;
        suzuki_sound_start_sfx(
            SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, channel), sound_id, 0x6000, 0x4000);
    }
}
