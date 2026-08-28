#include "fft/battle.h"

void battle_unit_snap_all_facings_to_quadrants(void) {
    battle_unit_misc_data_t* unit;
    s32 facing;

    unit = g_battle_unit_misc_list_head;
    while (unit != 0) {
        facing = unit->facing & BATTLE_FACING_MASK;
        if (facing < 0x200) {
            unit->facing = BATTLE_FACING_SOUTH;
        } else if (facing < 0x600) {
            unit->facing = BATTLE_FACING_WEST;
        } else if (facing < 0xa00) {
            unit->facing = BATTLE_FACING_NORTH;
        } else if (facing < 0xe00) {
            unit->facing = BATTLE_FACING_EAST;
        } else {
            unit->facing = BATTLE_FACING_SOUTH;
        }
        unit = unit->previous;
    }
}
