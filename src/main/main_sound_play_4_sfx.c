#include "fft/main_sound.h"
#include "psx/types.h"

/* Plays four sound effects on the SFX voice pairs 0, 2, 4 and 6. */
void main_sound_play_4_sfx(s32 first, s32 second, s32 third, s32 fourth) {
    if ((g_main_sound_driver_flags & 0x1000) != 0) {
        g_main_sound_sfx_channel_count = 2;
        main_sound_start_sfx(0x2000, first, 0x6000, 0x4000);
        main_sound_start_sfx(0x2002, second, 0x6000, 0x4000);
        main_sound_start_sfx(0x2004, third, 0x6000, 0x4000);
        main_sound_start_sfx(0x2006, fourth, 0x6000, 0x4000);
    }
}
