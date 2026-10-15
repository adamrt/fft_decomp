#include "fft/battle_unit.h"

void battle_unit_snap_all_facings_to_quadrants(void) {
    battle_unit_t* unit;
    s32 facing;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        facing = unit->facing & BATTLE_FACING_MASK;
        if (facing < BATTLE_FACING_SOUTH_WEST) {
            unit->facing = BATTLE_FACING_SOUTH;
        } else if (facing < BATTLE_FACING_NORTH_WEST) {
            unit->facing = BATTLE_FACING_WEST;
        } else if (facing < BATTLE_FACING_NORTH_EAST) {
            unit->facing = BATTLE_FACING_NORTH;
        } else if (facing < BATTLE_FACING_SOUTH_EAST) {
            unit->facing = BATTLE_FACING_EAST;
        } else {
            unit->facing = BATTLE_FACING_SOUTH;
        }
        unit = unit->previous;
    }
}
