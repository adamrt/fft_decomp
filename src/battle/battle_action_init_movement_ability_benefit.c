#include "fft/battle_move.h"
#include "fft/main_unit.h"

/*
 * Prepare the action record for a movement ability's benefit.
 *
 * Initialize the record, then select HP, MP, EXP or JP in that order.
 * Return -1 for a blocking status and 0 otherwise, including when no
 * movement benefit is present. The caller applies and displays the result.
 */
s32 battle_action_init_movement_ability_benefit(battle_stats_t* unit) {
    s32 flags;
    u8 unit_id;
    s32 amount;
    battle_action_data_t* action;

    unit_id = unit->misc_unit_id;
    g_battle_action_target = unit;
    g_battle_action_target_data = &unit->action;
    g_current_ability.target_id = unit_id;
    battle_action_clear_current_data(&unit->action);
    if (main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_PREVENT_REACTION))
        return -1;
    flags = battle_move_get_support_flags(unit);
    if (flags & BATTLE_MOVE_POST_EVENT_MOVE_HP_UP) {
        amount = (unit->max_hp + 9) / 10;
        action = g_battle_action_target_data;
        action->attack_type = BATTLE_ACTION_TYPE_HP_HEALING;
        action->hp_healing = amount;
    } else if (flags & BATTLE_MOVE_POST_EVENT_MOVE_MP_UP) {
        amount = (unit->max_mp + 9) / 10;
        action = g_battle_action_target_data;
        action->attack_type = BATTLE_ACTION_TYPE_MP_HEALING;
        action->mp_healing = amount;
    } else if (flags & BATTLE_MOVE_POST_EVENT_MOVE_GET_EXP) {
        g_battle_action_target_data->exp_change = battle_unit_calculate_move_exp_jp_up_to_earn(g_battle_action_target);
        if (g_battle_action_target->support_abilities[1] & BATTLE_SUPPORT_SET_2_GAINED_EXP_UP)
            g_battle_action_target_data->exp_change *= 2;
        g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_PSEUDO_STATUS;
    } else if (flags & BATTLE_MOVE_POST_EVENT_MOVE_GET_JP) {
        g_battle_action_target_data->jp_change = battle_unit_calculate_move_exp_jp_up_to_earn(g_battle_action_target);
        if (g_battle_action_target->support_abilities[1] & BATTLE_SUPPORT_SET_2_GAINED_JP_UP)
            g_battle_action_target_data->jp_change *= 2;
        g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_PSEUDO_STATUS;
    }
    return 0;
}
