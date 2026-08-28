#include "fft/battle.h"
#include "fft/battle_effect.h"

/* Add random X/Z/Y offsets and convert each result to Q12 coordinates. */
void battle_effect_add_random_vector_offsets_and_store_q12(
    const VECTOR* source, const VECTOR* ranges, VECTOR* destination) {
    u16 range;

    range = ranges->vx;
    destination->vx = (source->vx + battle_effect_get_random_between_s16((s16)-range, (s16)range)) << 12;

    range = ranges->vy;
    destination->vy = (source->vy + battle_effect_get_random_between_s16((s16)-range, (s16)range)) << 12;

    range = ranges->vz;
    destination->vz = (source->vz + battle_effect_get_random_between_s16((s16)-range, (s16)range)) << 12;
}
