#include "fft/battle.h"
#include "psx/types.h"

/* Quadratic height term used by the projectile trajectory calculation;
 * this is not the complete trajectory.
 * distance is the attacker-to-target distance scaled by ONE. */
s32 battle_effect_calculate_arc_height_term(s32 distance) {
    s32 gravity;

    distance >>= 6;
    gravity = g_battle_effect_gravity_modifier;
    return gravity / 2 - (distance * distance) / (gravity >> 11);
}
