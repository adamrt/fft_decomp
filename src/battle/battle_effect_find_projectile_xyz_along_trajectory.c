#include "fft/battle.h"

void battle_effect_find_projectile_xyz_along_trajectory(const VECTOR* src, s32 total, s32 current, VECTOR* dst) {
    s32 progress;

    progress = (current << 8) / (total >> 4);
    dst->vx = (progress * src->vx) >> 12;
    dst->vy = (progress * src->vy) >> 12;
    dst->vz = (progress * src->vz) >> 12;
}
