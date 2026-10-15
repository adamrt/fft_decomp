#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/* Count down the unit's timed statuses, Poison through Reflect (Death Sentence
 * is not among them), and queue the removal of each that runs out. */
void status_update_expiring(s32 unit_id) {
    s32 i;
    s32 byte_idx;
    s32 mask;
    unit_stats_t* target;
    action_data_t* action;
    s32 remaining_ct;

    action_set_target_variables(&g_battle_unit_stats[unit_id]);

    /* Statuses 24..38 (bytes 3 and 4): tick the CT of each inflicted,
       non-innate status and flag it for removal when it reaches zero. */
    for (i = 0; i < 15; i++) {
        byte_idx = (i / 8) + 3;
        target = g_action_target;
        if (target->inflicted_status[byte_idx] & (mask = 0x80 >> (i & 7))) {
            if ((target->status_sets.innate[byte_idx] & mask) == 0) {
                remaining_ct = target->status_ct[i] - 1;
                target->status_ct[i] = remaining_ct;
                if ((remaining_ct & 0xFF) == 0) {
                    action = g_action_target_data;
                    action->status_changes.removal[byte_idx] = mask | action->status_changes.removal[byte_idx];
                }
            }
        }
    }

    if (status_modify_inflictions(0) != 0) {
        g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
    }
}
