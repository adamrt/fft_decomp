#include "fft/suzuki.h"
#include "psx/libspu.h"
#include "psx/types.h"

/* Builds an SpuVolume pair from volume for the current sound type.
 *
 * Mono and stereo use volume on both sides. The wide types phase-invert one
 * side: type 2 (0x300) the right side for mode 0 (master and CD volume) and
 * the left for mode 1 (reverb depth), type 3 (0x500) the opposite.
 *
 * volume is s32: an s16 parameter adds a copy of a0. The type 3 test stores
 * `mode != 1` as a value (`xori` + `bnez`); as a branch condition GCC loads
 * the constant (`li` + `bne`). */
void suzuki_sound_set_vol_balance(s32 volume, SpuVolume* volume_out, u8 mode) {
    s32 invert_left;

    volume_out->right = volume;
    volume_out->left = volume;
    if (g_suzuki_sound_driver_flags & (SUZUKI_SOUND_DRIVER_WIDE_OPPOSITE | SUZUKI_SOUND_DRIVER_WIDE)) {
        invert_left = mode;
        if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_WIDE) == 0) {
            invert_left = invert_left != 1;
            if (invert_left) {
                volume_out->left = -volume;
            } else {
                volume_out->right = -volume;
            }
        } else if (invert_left) {
            volume_out->left = -volume;
        } else {
            volume_out->right = -volume;
        }
    }
}
