#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/*
 * Remove the statuses the unit's action marks for removal.
 *
 * Only statuses the unit currently has are kept in the removal mask; each one
 * gets its CT cleared and its special flags dropped, then the bits are cleared
 * from both the current and innate status sets.
 */
void status_apply_unit_action_removal(unit_stats_t* unit) {
    s32 i;
    s32 battle_id;
    s32 idx;
    s32 mask;

    g_action_target_data = &unit->action;
    battle_id = unit->battle_id;
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        g_action_target_data->status_changes.removal[i] &= unit->status_sets.current[i];
    }
    for (i = 0; i < STATUS_COUNT; i++) {
        idx = i / 8;
        mask = 0x80 >> (i & 7);
        if (g_action_target_data->status_changes.removal[idx] & mask) {
            main_status_set_ct(unit, i, 0);
            status_enable_special_flags(STATUS_HANDLER_INDEX(i), 0, battle_id);
        }
    }
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        unit->status_sets.current[i] &= ~g_action_target_data->status_changes.removal[i];
        unit->status_sets.innate[i] &= ~g_action_target_data->status_changes.removal[i];
    }
}
