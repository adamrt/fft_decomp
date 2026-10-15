#include "fft/battle_unit.h"

/* Request an animation and set the facing: battle_gfx_update_and_animate_unit_wep_eff
 * starts the animation on a later frame, and not at all while its sequence
 * is the one playing. The request is stored as the id + 1. */
void battle_unit_store_animation_facing(u32 animation, s32 facing, battle_unit_t* unit) {
    animation++;
    unit->requested_animation = animation;
    unit->facing = facing;
}
