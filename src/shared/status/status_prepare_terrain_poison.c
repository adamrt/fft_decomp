#include "fft/battle_action.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"

/* Prepare Poison Marsh's automatic status action for a grounded unit.
 *
 * Mounts cannot receive the terrain effect. A rider uses the linked mount's
 * Float status because that unit determines whether the pair touches the
 * terrain. The resident terrain table marks surfaces that inflict Poison. */
s32 status_prepare_terrain_poison(unit_stats_t* unit) {
    s32 mount_info;
    u8 unit_id;
    action_data_t* action;

    mount_info = unit->mount_info;
    if (status_check_crystal_dead_jump_petrify_treasure(unit))
        return 0;
    if (mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)
        return 0;
    if (mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) {
        if (g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK]
                .status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & STATUS_BYTE_MASK(STATUS_ID_FLOAT))
            return 0;
    } else if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
        return 0;
    }
    action = &unit->action;
    unit_id = unit->battle_id;
    g_action_target = unit;
    g_action_target_data = action;
    g_action_current_ability.target_id = unit_id;
    action_clear_current_data(action);
    if (g_main_terrain_status_flags[g_map_tile_data[map_calculate_location(unit)].surface.type]
        & MAIN_TERRAIN_STATUS_FLAG_POISON) {
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_POISON)]
            = STATUS_BYTE_MASK(STATUS_ID_POISON);
        if (status_modify_inflictions(0)) {
            g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
            return 1;
        }
    }
    return 0;
}
