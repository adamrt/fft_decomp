#include "fft/main.h"
#include "psx/cd.h"
#include "psx/types.h"

/* Sets the CD audio attenuation for the current sound type;
 * level is 0xc0 at start-up.
 *
 * For stereo and wide (status bits 0x700) each CD channel feeds its own side
 * at level; for mono both channels feed both sides at 160/255 of it. The
 * two direct stores sit in each branch: in the join block CSE would share
 * the val0 address register with the CdMix argument. */
void main_sound_put_type(s16 level) {
    s16 cross;

    g_main_sound_spu_state.sound_type = level;
    if (g_main_sound_driver_flags & 0x700) {
        cross = 0;
        g_main_sound_cd_mix.val0 = g_main_sound_cd_mix.val2 = level;
    } else {
        cross = level * 160 / 255;
        g_main_sound_cd_mix.val0 = g_main_sound_cd_mix.val2 = cross;
    }
    g_main_sound_cd_mix_left_to_right = g_main_sound_cd_mix_right_to_left = cross;
    CdMix(&g_main_sound_cd_mix);
}
