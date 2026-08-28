#include "psx/types.h"

void battle_effect_find_projectile_xyz_along_trajectory(s32* src, s32 total, s32 current, s32* dst) {
    s32 progress;

    progress = (current << 8) / (total >> 4);
    dst[0] = (progress * src[0]) >> 12;
    dst[1] = (progress * src[1]) >> 12;
    dst[2] = (progress * src[2]) >> 12;
}
