#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_interpolate_svector(const s16* from, const s16* to, s32 t, s16* out) {
    battle_effect_interpolate_s16(from[0], to[0], t, &out[0]);
    battle_effect_interpolate_s16(from[1], to[1], t, &out[1]);
    battle_effect_interpolate_s16(from[2], to[2], t, &out[2]);
}
