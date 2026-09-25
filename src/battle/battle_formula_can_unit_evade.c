#include "fft/battle.h"
#include "psx/types.h"

s32 battle_formula_can_unit_evade(battle_stats_t* unit) {
    s32 depth;

    if ((unit->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DONT_ACT)) != 0) {
        return 1;
    }
    if ((unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) != 0) {
        return 1;
    }
    depth = g_battle_map_tile_data[battle_map_calculate_location(unit)].depth_half_height >> MAP_TILE_DEPTH_SHIFT;
    if (depth < 2) {
        return 0;
    }
    if ((unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_FLOAT)]
            & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FLOAT) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHICKEN)
                | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG)))
        != 0) {
        return 0;
    }
    if ((unit->movement_abilities[2]
            & (BATTLE_MOVEMENT_SET_3_WALK_ON_WATER | BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER | BATTLE_MOVEMENT_SET_3_FLOAT))
        != 0) {
        return 0;
    }
    return ((unit->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) == 0) << 1;
}
