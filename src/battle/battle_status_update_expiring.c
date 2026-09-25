#include "fft/battle.h"
#include "psx/types.h"

void battle_status_update_expiring(s32 unit_id) {
    s32 i;
    s32 byte_idx;
    s32 mask;
    battle_stats_t* target;
    battle_action_data_t* action;
    s32 remaining_ct;

    battle_action_set_target_variables(&g_battle_unit_stats[unit_id]);

    /* Statuses 24..38 (bytes 3 and 4): tick the CT of each inflicted,
       non-innate status and flag it for removal when it reaches zero. */
    for (i = 0; i < 15; i++) {
        byte_idx = (i / 8) + 3;
        target = g_battle_action_target;
        if (target->inflicted_status[byte_idx] & (mask = 0x80 >> (i & 7))) {
            if ((target->status_sets.innate[byte_idx] & mask) == 0) {
                remaining_ct = target->status_ct[i] - 1;
                target->status_ct[i] = remaining_ct;
                if ((remaining_ct & 0xFF) == 0) {
                    action = g_battle_action_target_data;
                    action->status_removal[byte_idx] = mask | action->status_removal[byte_idx];
                }
            }
        }
    }

    if (battle_status_modify_inflictions(0) != 0) {
        g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_STATUS_CHANGE;
    }
}
