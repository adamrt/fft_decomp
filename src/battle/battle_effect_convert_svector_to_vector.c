#include "psx/gte.h"
#include "psx/types.h"

void battle_effect_convert_svector_to_vector(s16* src, VECTOR* dst) {
    dst->vx = src[0];
    dst->vy = src[1];
    dst->vz = src[2];
}
