#include "fft/battle_unit.h"

/* battle_unit_store_animation_facing with the force flag: the animation
 * starts on the next update, from its beginning even when it is playing. */
void battle_unit_store_animation_facing_movement_data(s32 animation, s32 facing, battle_unit_t* unit) {
    animation++;
    unit->requested_animation = animation;
    unit->facing = facing;
    unit->movement.force_animation_update = 1;
}
