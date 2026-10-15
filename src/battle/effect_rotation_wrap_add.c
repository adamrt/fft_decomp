/* Rotation add with wrap plus the unused halfword adders. */

#include "fft/battle_effect.h"
#include "psx/types.h"

enum {
    BATTLE_ROTATION_Q12_MASK = 0x0FFF,
};

/* Add two effect rotations and wrap each axis to one Q12 revolution. */
void effect_add_rotation_vectors_mod_q12(const SVECTOR* first, const SVECTOR* second, SVECTOR* out) {
    out->vx = (first->vx + second->vx) & BATTLE_ROTATION_Q12_MASK;
    out->vy = (first->vy + second->vy) & BATTLE_ROTATION_Q12_MASK;
    out->vz = (first->vz + second->vz) & BATTLE_ROTATION_Q12_MASK;
}

/* Nothing in the game calls this or its three siblings for 4, 6 and 8. */
void effect_add_2_to_halfword(u16* value) {
    *value += 2;
}

/* Nothing in the game calls it (see effect_add_2_to_halfword). */
void effect_add_4_to_halfword(u16* value) {
    *value += 4;
}

/* Nothing in the game calls it (see effect_add_2_to_halfword). */
void effect_add_6_to_halfword(u16* value) {
    *value += 6;
}

/* Nothing in the game calls it (see effect_add_2_to_halfword). */
void effect_add_8_to_halfword(u16* value) {
    *value += 8;
}
