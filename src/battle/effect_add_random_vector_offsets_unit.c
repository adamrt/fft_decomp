#include "fft/battle_effect.h"

void effect_add_random_vector_offsets(const VECTOR* source, const VECTOR* ranges, VECTOR* destination) {
    u16 range;

    range = ranges->vx;
    destination->vx = source->vx + effect_get_random_between_s16(-range, range);

    range = ranges->vy;
    destination->vy = source->vy + effect_get_random_between_s16(-range, range);

    range = ranges->vz;
    destination->vz = source->vz + effect_get_random_between_s16(-range, range);
}

/* Add random X/Z/Y offsets and convert each result to Q12 coordinates. */
void effect_add_random_vector_offsets_and_store_q12(const VECTOR* source, const VECTOR* ranges, VECTOR* destination) {
    u16 range;

    range = ranges->vx;
    destination->vx = (source->vx + effect_get_random_between_s16(-range, range)) * ONE;

    range = ranges->vy;
    destination->vy = (source->vy + effect_get_random_between_s16(-range, range)) * ONE;

    range = ranges->vz;
    destination->vz = (source->vz + effect_get_random_between_s16(-range, range)) * ONE;
}

void effect_add_vector_and_store_q12(const VECTOR* delta, VECTOR* value, VECTOR* out) {
    value->vx += delta->vx;
    out->vx = value->vx * ONE;
    value->vy += delta->vy;
    out->vy = value->vy * ONE;
    value->vz += delta->vz;
    out->vz = value->vz * ONE;
}

/* Add independent random offsets to three packed rotation components. */
void effect_add_random_rotation_offsets(const SVECTOR* source, const SVECTOR* ranges, SVECTOR* destination) {
    u16 range;

    range = ranges->vx;
    destination->vx = source->vx + effect_get_random_between_s16(-range, range);

    range = ranges->vy;
    destination->vy = source->vy + effect_get_random_between_s16(-range, range);

    range = ranges->vz;
    destination->vz = source->vz + effect_get_random_between_s16(-range, range);
}
