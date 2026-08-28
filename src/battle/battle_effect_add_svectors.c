#include "psx/types.h"

void battle_effect_add_svectors(const u16* first, const u16* second, u16* out) {
    out[0] = first[0] + second[0];
    out[1] = first[1] + second[1];
    out[2] = first[2] + second[2];
}
