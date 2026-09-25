#include "fft/main.h"
#include "psx/types.h"

/* Plays a sound effect on a free pair of SFX voices at the default volume
 * (0x60) and centre balance (0x40). */
void SuzukiPlaySoundFindChannel(s32 sound_id) {
    if ((g_main_sound_driver_flags & 0x1000) != 0) {
        s32 channel = main_sound_find_sfx_voice(sound_id, 2);
        g_main_sound_sfx_channel_count = 2;
        main_sound_start_sfx(channel | 0x2000, sound_id, 0x6000, 0x4000);
    }
}
