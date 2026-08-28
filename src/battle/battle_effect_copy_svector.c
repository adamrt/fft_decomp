#include "psx/types.h"

void battle_effect_copy_svector(u16* src, u16* dst) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}
