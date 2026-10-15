#include "fft/battle_unit.h"

/* Faces a unit in a direction given in quarter turns and restarts its animation
 * with that facing, as battle_unit_set_animation_from_facing_nibble does for
 * sixteenths. No retail code calls it. */
s32 battle_unit_set_animation_from_facing_hint(u32 runtime_id, s32 facing_hint) {
    battle_unit_t* unit;
    s32 animation;
    s16 facing;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        animation = unit->animation.sequence_id >> 1;
        facing = facing_hint << 10;
        unit->facing = facing;
        if (animation < BATTLE_UNIT_ANIMATION_IDLE) {
            animation = BATTLE_UNIT_ANIMATION_STAND;
        }
        battle_unit_store_animation_facing_movement_data(animation, facing, unit);
        return runtime_id;
    }
    return -1;
}
