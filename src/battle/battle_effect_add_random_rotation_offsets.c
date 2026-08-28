#include "fft/effect.h"

/* Add independent random offsets to three packed rotation components. */
void battle_effect_add_random_rotation_offsets(const battle_effect_rotation_vector_t* source,
    const battle_effect_rotation_vector_t* ranges, battle_effect_rotation_vector_t* destination) {
    u16 range;

    range = ranges->x;
    destination->x = source->x + battle_effect_get_random_between_s16((s16)-range, (s16)range);

    range = ranges->y;
    destination->y = source->y + battle_effect_get_random_between_s16((s16)-range, (s16)range);

    range = ranges->z;
    destination->z = source->z + battle_effect_get_random_between_s16((s16)-range, (s16)range);
}
