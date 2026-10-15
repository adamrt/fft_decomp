#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/* Set when a player-team unit becomes Crystal or Dead. */

/* Applies the pending status inflictions of unit_id's current action.
 *
 * For each of the 40 statuses the action inflicts that none of the unit's
 * inflicted statuses blocks, the status's cancel list becomes the action's
 * removal set, the pending changes are filtered, the status is recorded as
 * inflicted and given its CT, and its special flags are enabled (mode 2 when
 * re-inflicting anything but Crystal or Oil). Transparent clears the removal
 * flag; Invite and Charm copy the attacker's team, Invite permanently. Ends by
 * storing the unit's current statuses. */
void status_store_for_current_attack(s32 unit_id, s32 removal_only) {
    s32 i;
    s32 j;
    s32 byte;
    u8 mask;
    s32 blocked;
    u8 previous;

    g_action_current_ability.target_id = unit_id;
    g_action_target = &g_battle_unit_stats[unit_id];
    g_action_target_data = &g_battle_unit_stats[unit_id].action;
    for (i = 0; i < STATUS_COUNT; i++) {
        byte = i / 8;
        /* Signed shift (srav); STATUS_BYTE_MASK shifts unsigned. */
        mask = 0x80 >> (i & 7);
        if (!(g_action_target_data->status_changes.infliction[byte] & mask)) {
            continue;
        }
        blocked = 0;
        for (j = 0; j < STATUS_BYTE_COUNT; j++) {
            if (g_action_target->inflicted_status[j] & g_main_status_effect_data[i].cant_stack[j]) {
                blocked = 1;
                break;
            }
        }
        if (blocked != 0) {
            continue;
        }
        for (j = 0; j < STATUS_BYTE_COUNT; j++) {
            g_action_target_data->status_changes.removal[j] = g_main_status_effect_data[i].cancels[j];
        }
        status_modify_inflictions(removal_only);
        status_set_inflicted_ct_and_transfer_last_used_ct(unit_id);
        previous = g_action_target->inflicted_status[byte];
        g_action_target->inflicted_status[byte] = previous | mask;
        if (main_status_set_ct(g_action_target, i, 0) != 0) {
            continue;
        }
        if ((previous & mask) && i != STATUS_ID_CRYSTAL && i != STATUS_ID_OIL) {
            status_enable_special_flags(STATUS_HANDLER_INDEX(i), 2, unit_id);
        } else {
            status_enable_special_flags(STATUS_HANDLER_INDEX(i), 1, unit_id);
        }
        if (i == STATUS_ID_TRANSPARENT) {
            g_action_target->transparent_removal_flag = 0;
        }
        if (i == STATUS_ID_INVITE || i == STATUS_ID_CHARM) {
            previous = (g_action_target->team_flags & ~BATTLE_TEAM_MASK)
                | (g_action_attacker->team_flags & BATTLE_TEAM_MASK);
            g_action_target->team_flags = previous;
            if (i == STATUS_ID_INVITE) {
                previous = (g_action_target->initial_team_flags & ~BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)
                    | (g_action_attacker->team_flags & BATTLE_TEAM_MASK);
                g_action_target_data->reaction_id = 0;
                g_action_target->initial_team_flags = previous;
                g_action_target->auto_battle_setting = AI_AUTOBATTLE_MANUAL;
                g_action_target->auto_battle_target = 0;
            }
        }
        if (!(g_action_target->initial_team_flags & BATTLE_TEAM_MASK) && (i == 1 || i == 2)) {
            g_battle_player_unit_fallen = 1;
        }
    }
    main_status_store_current(g_action_target);
}
