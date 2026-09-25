#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_shift_vector_left_12(const VECTOR* src, VECTOR* dst) {
    dst->vx = src->vx << 12;
    dst->vy = src->vy << 12;
    dst->vz = src->vz << 12;
}
