#include "fft/battle.h"
#include "psx/types.h"

/* Linear counterpart of battle_effect_interpolate_vector_q13_cos: t = current/duration in q13. */
void battle_effect_interpolate_vector_q13_linear(const s32* from, const s32* to, s32 duration, s32 current, s32* out) {
    battle_effect_interpolate_vector_q13(from, to, (current << 13) / duration, out);
}
