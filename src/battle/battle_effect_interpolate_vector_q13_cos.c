#include "psx/gte.h"
#include "psx/types.h"

void battle_effect_interpolate_vector_q13_cos(const s32* from, const s32* to, s32 duration, s32 current, s32* out) {
    battle_effect_interpolate_vector_q13(from, to, ONE - rcos((current << 11) / duration), out);
}
