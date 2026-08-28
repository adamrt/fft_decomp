#include "psx/gte.h"
#include "psx/types.h"

void battle_effect_interpolate_svector_cos(const s16* from, const s16* to, s32 duration, s32 current, s16* out) {
    battle_effect_interpolate_svector(from, to, ONE - rcos((current << 11) / duration), out);
}
