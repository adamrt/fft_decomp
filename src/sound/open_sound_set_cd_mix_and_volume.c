#include "fft/open.h"
#include "psx/types.h"

/* Fades the CD audio (movie streams and XA) in or out over duration ticks.
 * A non-zero cd_mix_level is the CdMix attenuation suzuki_sound_put_type sets
 * for the current sound type, with the SPU's CD volume at 0x6400; 0 fades
 * that volume to 0 and leaves the attenuation alone. */
void open_sound_set_cd_mix_and_volume(s32 cd_mix_level, s32 duration) {
    s32 volume;

    if (cd_mix_level == 0) {
        volume = 0;
    } else {
        suzuki_sound_put_type(cd_mix_level);
        volume = 0x6400;
    }

    suzuki_sound_set_cd_volume(volume, duration);
}
