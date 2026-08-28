#include "fft/effect.h"

enum {
    BATTLE_ROTATION_Q12_MASK = 0x0FFF,
};

/* Add two effect rotations and wrap each axis to one Q12 revolution. */
void battle_effect_add_rotation_vectors_mod_q12(const battle_effect_rotation_vector_t* first,
    const battle_effect_rotation_vector_t* second, battle_effect_rotation_vector_t* out) {
    out->x = (first->x + second->x) & BATTLE_ROTATION_Q12_MASK;
    out->y = (first->y + second->y) & BATTLE_ROTATION_Q12_MASK;
    out->z = (first->z + second->z) & BATTLE_ROTATION_Q12_MASK;
}
