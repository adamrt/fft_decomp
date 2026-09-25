#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_interpolate_vector_q13(const s32* from, const s32* to, s32 t, s32* out) {
    battle_effect_lerp_component_q13(from[0], to[0], t, &out[0]);
    battle_effect_lerp_component_q13(from[1], to[1], t, &out[1]);
    battle_effect_lerp_component_q13(from[2], to[2], t, &out[2]);
}
