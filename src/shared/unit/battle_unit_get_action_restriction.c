#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Returns the unit's battle_unit_action_restriction_e.
 *
 * CANNOT_ACT for Don't Act or a ridden mount. DEEP_WATER on a tile of depth 2 or
 * more, unless the unit has Float, Chicken or Frog, one of the Move in Water,
 * Walk on Water or Float movement abilities, or is a rider. */
battle_unit_action_restriction_e battle_unit_get_action_restriction(const unit_stats_t* unit) {
    s32 depth;

    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_ACT)] & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT))
        != 0) {
        return BATTLE_UNIT_ACTION_RESTRICTION_CANNOT_ACT;
    }
    if ((unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) != 0) {
        return BATTLE_UNIT_ACTION_RESTRICTION_CANNOT_ACT;
    }
    depth = g_map_tile_data[map_calculate_location(unit)].depth_half_height.depth;
    if (depth < 2) {
        return BATTLE_UNIT_ACTION_RESTRICTION_NONE;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & (STATUS_BYTE_MASK(STATUS_ID_FLOAT) | STATUS_BYTE_MASK(STATUS_ID_CHICKEN)
                | STATUS_BYTE_MASK(STATUS_ID_FROG)))
        != 0) {
        return BATTLE_UNIT_ACTION_RESTRICTION_NONE;
    }
    if ((unit->ability_flags.movement[2]
            & (BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER | BATTLE_MOVEMENT_SET_3_WALK_ON_WATER | BATTLE_MOVEMENT_SET_3_FLOAT))
        != 0) {
        return BATTLE_UNIT_ACTION_RESTRICTION_NONE;
    }
    return ((unit->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) == 0) << 1;
}
