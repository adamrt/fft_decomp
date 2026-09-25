#include "fft/battle.h"
#include "psx/types.h"

/*
 * Apply the current ability's status set to the target action.
 *
 * All-or-nothing copies the full set, random chooses one status, separate
 * rolls each status independently, and cancel writes the removal set. Preview
 * and AI evaluation force probabilistic modes through the all-or-nothing path.
 */
void battle_formula_apply_status_to_action(void) {
    s32 status_ids[BATTLE_STATUS_COUNT];
    s32 type;
    s32 count;
    s32 status;
    s32 selected_status;
    s32 byte;
    s32 mask;
    s32 any;
    battle_action_data_t* action;

    type = g_current_ability.status_infliction.type;
    if (type & BATTLE_STATUS_INFLICTION_TYPE_SEPARATE) {
        *(s16*)&g_battle_action_target_data->attack_accuracy /= 4;
    }
    if (type & (BATTLE_STATUS_INFLICTION_TYPE_SEPARATE | BATTLE_STATUS_INFLICTION_TYPE_RANDOM_ONE)) {
        if (g_battle_action_state != BATTLE_ACTION_STATE_EXECUTE) {
            type = BATTLE_STATUS_INFLICTION_TYPE_ALL_OR_NOTHING;
        }
    }

    if (type & BATTLE_STATUS_INFLICTION_TYPE_ALL_OR_NOTHING) {
        status = 0;
        do {
            g_battle_action_target_data->status_infliction[status]
                |= g_current_ability.status_infliction.statuses[status];
            status++;
        } while (status < BATTLE_STATUS_BYTE_COUNT);
    } else if (type & BATTLE_STATUS_INFLICTION_TYPE_RANDOM_ONE) {
        count = 0;
        status = 0;
        do {
            if (g_current_ability.status_infliction.statuses[status / 8] & (0x80 >> (status & 7))) {
                status_ids[count] = status;
                count++;
            }
            status++;
        } while (status < BATTLE_STATUS_COUNT);

        selected_status = status_ids[(battle_formula_get_random_0_7fff() * count) / 32768];
        g_battle_action_target_data->status_infliction[selected_status / 8] |= 0x80 >> (selected_status & 7);
    } else if (type & BATTLE_STATUS_INFLICTION_TYPE_SEPARATE) {
        status = 0;
        do {
            byte = status / 8;
            if (g_current_ability.status_infliction.statuses[byte] & (mask = 0x80 >> (status & 7))) {
                if (main_util_roll_pass_fail(100, 24) != 0 || g_battle_action_state == BATTLE_ACTION_STATE_PREVIEW) {
                    g_current_ability.status_infliction.statuses[byte] ^= mask;
                }
            }
            status++;
        } while (status < BATTLE_STATUS_COUNT);

        status = 0;
        do {
            g_battle_action_target_data->status_infliction[status]
                |= g_current_ability.status_infliction.statuses[status];
            status++;
        } while (status < BATTLE_STATUS_BYTE_COUNT);
    } else if (type & BATTLE_STATUS_INFLICTION_TYPE_CANCEL) {
        status = 0;
        do {
            g_battle_action_target_data->status_removal[status] |= g_current_ability.status_infliction.statuses[status];
            status++;
        } while (status < BATTLE_STATUS_BYTE_COUNT);
    }

    any = 0;
    status = 0;
    action = g_battle_action_target_data;
    do {
        any |= action->status_infliction[status] | action->status_removal[status];
        status++;
    } while (status < BATTLE_STATUS_BYTE_COUNT);

    if ((u8)any == 0 || battle_status_modify_inflictions(0) == 0) {
        battle_formula_force_attack_miss();
        return;
    }

    g_battle_action_target_data->attack_type |= BATTLE_ACTION_TYPE_STATUS_CHANGE;
    if (g_battle_action_target_data->status_infliction[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_INVITE)) {
        g_battle_action_target_data->special_effect |= BATTLE_ACTION_SPECIAL_EFFECT_TEAM_CHANGE;
    }
}
