#include "fft/main_sound.h"
#include "psx/types.h"

/* SuzukiPlaySoundFindChannel with explicit volume and balance bytes; SMD
 * opcode 0x9C (main_smd_play_sfx) passes balance 0x40 (centre). */
void main_sound_play_sfx_with_settings_find_channel(s32 sound_id, s32 volume, s32 balance) {
    if ((g_main_sound_driver_flags & 0x1000) != 0) {
        s32 channel = main_sound_find_sfx_voice(sound_id, 2);
        g_main_sound_sfx_channel_count = 2;
        main_sound_start_sfx(channel | 0x2000, sound_id, volume << 8, balance << 8);
    }
}
