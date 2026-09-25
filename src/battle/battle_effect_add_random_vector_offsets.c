#include "fft/battle.h"

/* Add an independent signed random offset to each X/Z/Y component. */
void battle_effect_add_random_vector_offsets(const VECTOR* source, const VECTOR* ranges, VECTOR* destination) {
    u16 range;

    range = ranges->vx;
    destination->vx = source->vx + battle_effect_get_random_between_s16((s16)-range, (s16)range);

    range = ranges->vy;
    destination->vy = source->vy + battle_effect_get_random_between_s16((s16)-range, (s16)range);

    range = ranges->vz;
    destination->vz = source->vz + battle_effect_get_random_between_s16((s16)-range, (s16)range);
}
