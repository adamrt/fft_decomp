#include "fft/battle.h"
#include "psx/types.h"

/*
 * Apply the pending status infliction/removal on battle unit unit_id.
 *
 * The current target's ten status bytes are saved around the resolution and
 * restored afterwards, since this runs from the event thread
 * (battle_status_inflict_by_entd_unit_id) in the middle of an action.
 */
s32 battle_status_resolve_unit_changes(s32 unit_id, s32 removal_only) {
    u8 saved[10];

    if (g_battle_unit_stats[unit_id].entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    main_util_copy_byte_data(g_battle_action_target_data->status_infliction, saved, 10);
    g_current_ability.target_id = unit_id;
    g_battle_action_target = &g_battle_unit_stats[unit_id];
    g_battle_action_target_data = &g_battle_unit_stats[unit_id].action;
    battle_status_modify_inflictions(removal_only);
    battle_status_set_inflicted_ct_and_transfer_last_used_ct(unit_id);
    battle_status_store_for_current_attack(unit_id, removal_only);
    if (g_battle_action_target->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD)) {
        g_battle_action_target->hp = 0;
    }
    if (!(g_battle_action_target->status_sets.current[0]
            & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP)
                | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PERFORMING)))) {
        g_battle_action_target->charged_ability_ct = 0xFF;
    }
    battle_status_remove_control(g_battle_action_target);
    main_util_copy_byte_data(saved, g_battle_action_target_data->status_infliction, 10);
    return 0;
}
