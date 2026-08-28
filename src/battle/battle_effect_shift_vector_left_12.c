#include "fft/battle_effect.h"
#include "psx/types.h"

void battle_effect_shift_vector_left_12(const s32* src, s32* dst) {
    dst[0] = src[0] << 12;
    dst[1] = src[1] << 12;
    dst[2] = src[2] << 12;
}
