#include "fft/battle_unit.h"

/* Turn a unit to one of the four cardinal facings and restart its animation.
 * A diagonal or any other angle leaves it as it is. */
void battle_unit_decide_facing_direction(battle_unit_t* unit, s32 facing) {
    switch (facing) {
    case BATTLE_FACING_NORTH:
        if (unit->facing == BATTLE_FACING_NORTH) {
            return;
        }
        unit->facing = BATTLE_FACING_NORTH;
        break;
    case BATTLE_FACING_SOUTH:
        if (unit->facing == BATTLE_FACING_SOUTH) {
            return;
        }
        unit->facing = BATTLE_FACING_SOUTH;
        break;
    case BATTLE_FACING_WEST:
        if (unit->facing == BATTLE_FACING_WEST) {
            return;
        }
        unit->facing = BATTLE_FACING_WEST;
        break;
    case BATTLE_FACING_EAST:
        if (unit->facing == BATTLE_FACING_EAST) {
            return;
        }
        unit->facing = BATTLE_FACING_EAST;
        break;
    case BATTLE_FACING_SOUTH_WEST:
    case BATTLE_FACING_NORTH_WEST:
    case BATTLE_FACING_NORTH_EAST:
    case BATTLE_FACING_SOUTH_EAST:
    default:
        return;
    }
    battle_unit_set_animation_based_on_status(unit);
}
