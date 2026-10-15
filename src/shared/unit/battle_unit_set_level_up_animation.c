#include "fft/battle_move.h"
#include "fft/battle_unit.h"

/* Start a unit's level-up animation: the jump on dry land, the hops for a
 * rider and in water one deep, the submerged idle in water two deep. */
void battle_unit_set_level_up_animation(battle_unit_t* unit) {
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_LEVEL_UP_HOP, unit->facing, unit);
        return;
    }
    switch (move_get_water_depth(unit)) {
    case 0:
        battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_LEVEL_UP, unit->facing, unit);
        break;
    case 1:
        battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_LEVEL_UP_HOP, unit->facing, unit);
        break;
    case 2:
        battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
        break;
    }
}
