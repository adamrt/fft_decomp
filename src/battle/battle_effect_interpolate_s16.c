#include "psx/types.h"

void battle_effect_interpolate_s16(s16 from, s16 to, s32 t, s16* out) {
    *out = from + (((to - from) * t) / 0x2000);
}
