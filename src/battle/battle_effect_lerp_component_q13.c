#include "psx/types.h"

void battle_effect_lerp_component_q13(s32 from, s32 to, s32 t, s32* out) {
    *out = from + (to - from) * t / 0x2000;
}
