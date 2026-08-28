#include "psx/types.h"

/* Linear counterpart of battle_effect_interpolate_svector_cos: t = current/duration in q13. */
void battle_effect_interpolate_svector_linear(const s16* from, const s16* to, s32 duration, s32 current, s16* out) {
    battle_effect_interpolate_svector(from, to, (current << 13) / duration, out);
}
