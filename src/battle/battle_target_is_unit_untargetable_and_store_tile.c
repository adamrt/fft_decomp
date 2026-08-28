#include "fft/battle.h"

/* Store a unit's tile and report whether targeting must reject the unit. */
s32 battle_target_is_unit_untargetable_and_store_tile(s32 unit_id, s32* tile_index) {
    battle_stats_t* unit = &g_battle_unit_stats[unit_id];
    s32 result;

    *tile_index = battle_map_calculate_location(unit);
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        result = 1;
    } else if (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CRYSTAL)]
        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP))) {
        result = 1;
    } else if (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_TREASURE)]
        & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE)) {
        result = 1;
    } else if (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
        result = 1;
    } else {
        result = 0;
    }
    return result;
}
