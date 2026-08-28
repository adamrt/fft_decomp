#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "fft/map.h"

/* Prepare Poison Marsh's automatic status action for a grounded unit.
 *
 * Mounts cannot receive the terrain effect. A rider uses the linked mount's
 * Float status because that unit determines whether the pair touches the
 * terrain. The resident terrain table marks surfaces that inflict Poison. */
s32 battle_prepare_terrain_poison(battle_stats_t* unit) {
    s32 mount_info;
    u8 unit_id;
    battle_action_data_t* action;

    mount_info = unit->mount_info;
    if (battle_status_check_crystal_dead_jump_petrify_treasure(unit))
        return 0;
    if (mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)
        return 0;
    if (mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) {
        if (g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK]
                .status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_FLOAT)]
            & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FLOAT))
            return 0;
    } else if (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_FLOAT)]
        & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FLOAT)) {
        return 0;
    }
    action = &unit->action;
    unit_id = unit->misc_unit_id;
    g_battle_action_target = unit;
    g_battle_action_target_data = action;
    g_current_ability.target_id = unit_id;
    battle_action_clear_current_data(action);
    if (g_main_terrain_status_flags[g_battle_map_tile_data[battle_map_calculate_location(unit)].surface.bits.type]
        & 0x80) {
        g_battle_action_target_data->status_infliction[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_POISON)]
            = BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_POISON);
        if (battle_status_modify_inflictions(0)) {
            g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_STATUS_CHANGE;
            return 1;
        }
    }
    return 0;
}
