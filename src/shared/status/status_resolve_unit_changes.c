#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "fft/memory.h"
#include "psx/types.h"

/*
 * Apply the pending status infliction/removal on battle unit unit_id.
 *
 * The current target's ten status bytes are saved around the resolution and
 * restored afterwards, since this runs from the event thread
 * (status_inflict_by_event_unit_id) in the middle of an action.
 */
s32 status_resolve_unit_changes(s32 unit_id, s32 removal_only) {
    unit_action_status_changes_t saved;

    if (g_battle_unit_stats[unit_id].entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    mem_copy_byte_data_source_first(&g_action_target_data->status_changes, &saved, sizeof(saved));
    g_action_current_ability.target_id = unit_id;
    g_action_target = &g_battle_unit_stats[unit_id];
    g_action_target_data = &g_battle_unit_stats[unit_id].action;
    status_modify_inflictions(removal_only);
    status_set_inflicted_ct_and_transfer_last_used_ct(unit_id);
    status_store_for_current_attack(unit_id, removal_only);
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)) {
        g_action_target->hp = 0;
    }
    if (!(g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
            & (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
                | STATUS_BYTE_MASK(STATUS_ID_PERFORMING)))) {
        g_action_target->charged_ability_ct = 0xFF;
    }
    status_remove_control(g_action_target);
    mem_copy_byte_data_source_first(&saved, &g_action_target_data->status_changes, sizeof(saved));
    return 0;
}
