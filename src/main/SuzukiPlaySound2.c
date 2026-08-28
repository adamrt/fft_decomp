#include "fft/main_sound.h"
#include "psx/types.h"

void SuzukiPlaySound2(s32 sound) {
    if ((g_main_sound_driver_flags & 0x1000) != 0) {
        g_main_sound_sfx_channel_count = 2;
        main_sound_start_sfx(0x6004, sound, 0x6000, 0x4000);
    }
}
