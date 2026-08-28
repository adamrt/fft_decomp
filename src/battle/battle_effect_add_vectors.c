#include "psx/types.h"

void battle_effect_add_vectors(const s32* first, const s32* second, s32* out) {
    out[0] = first[0] + second[0];
    out[1] = first[1] + second[1];
    out[2] = first[2] + second[2];
}
