#include "fft/suzuki.h"
#include "psx/libcd.h"
#include "psx/types.h"

extern CdlATV g_suzuki_sound_cd_mix; /* 0x80032a3c; written by Put Sound Type */

/* Sets the CD audio attenuation for the current sound type;
 * level is 0xc0 at start-up.
 *
 * For stereo and wide (status bits 0x700) each CD channel feeds its own side
 * at level; for mono both channels feed both sides at 160/255 of it. The
 * two direct stores sit in each branch: in the join block CSE would share
 * the val0 address register with the CdMix argument. */
void suzuki_sound_put_type(s16 level) {
    s16 cross;
    CdlATV* mix;

    g_suzuki_sound_spu_state.cd_mix_level = level;
    if (g_suzuki_sound_driver_flags
        & (SUZUKI_SOUND_DRIVER_WIDE_OPPOSITE | SUZUKI_SOUND_DRIVER_WIDE | SUZUKI_SOUND_DRIVER_STEREO)) {
        cross = 0;
        g_suzuki_sound_cd_mix.val0 = g_suzuki_sound_cd_mix.val2 = level;
    } else {
        cross = level * 160 / 255;
        g_suzuki_sound_cd_mix.val0 = g_suzuki_sound_cd_mix.val2 = cross;
    }
    /* Assign the mix pointer before the crossfeed stores to retain their separate address bases. */
    mix = &g_suzuki_sound_cd_mix;
    g_suzuki_sound_cd_mix.val1 = g_suzuki_sound_cd_mix.val3 = cross;
    CdMix(mix);
}
