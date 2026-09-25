#include "fft/battle.h"
#include "psx/types.h"

/*
 * Remove the statuses the unit's action marks for removal.
 *
 * Only statuses the unit currently has are kept in the removal mask; each one
 * gets its CT cleared and its special flags dropped, then the bits are cleared
 * from both the current and innate status sets.
 */
void battle_status_apply_unit_action_removal(battle_stats_t* unit) {
    s32 i;
    s32 misc_unit_id;
    s32 idx;
    s32 mask;

    g_battle_action_target_data = &unit->action;
    misc_unit_id = unit->misc_unit_id;
    for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
        g_battle_action_target_data->status_removal[i] &= unit->status_sets.current[i];
    }
    for (i = 0; i < BATTLE_STATUS_COUNT; i++) {
        idx = i / 8;
        mask = 0x80 >> (i & 7);
        if (g_battle_action_target_data->status_removal[idx] & mask) {
            main_status_set_ct(unit, i, 0);
            battle_status_enable_special_flags(BATTLE_STATUS_HANDLER_INDEX(i), 0, misc_unit_id);
        }
    }
    for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
        unit->status_sets.current[i] &= ~g_battle_action_target_data->status_removal[i];
        unit->status_sets.innate[i] &= ~g_battle_action_target_data->status_removal[i];
    }
}
