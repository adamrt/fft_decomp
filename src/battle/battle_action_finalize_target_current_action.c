#include "fft/battle.h"
#include "psx/types.h"

/* Validate and clamp the target's current action result before it applies.
 *
 * A target that is dead (unless the ability cancels Dead or the target is
 * undead), petrified or walled, without the ability cancelling that status,
 * turns the action into a miss. Otherwise zero accuracy clears the hit flag,
 * HP/MP changes are capped at 999, a lethal HP hit drops knockback, and a
 * hit absorbed by the target team's Golem becomes a guarded miss. */
void battle_action_finalize_target_current_action(void) {
    battle_action_data_t* action;
    u8 status;
    s32 invalid;
    s32 team;

    status = g_battle_action_target->status_sets.current[0];
    invalid = 0;
    if ((status & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD))
        && (!(g_current_ability_canceled_statuses[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD))
            || (status & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_UNDEAD)))) {
        invalid = 1;
    }
    if ((g_battle_action_target->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PETRIFY))
        && !(g_current_ability_canceled_statuses[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PETRIFY))) {
        invalid = 1;
    }
    if ((g_battle_action_target->status_sets.current[3] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_WALL))
        && !(g_current_ability_canceled_statuses[3] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_WALL))) {
        invalid = 1;
    }
    if (invalid != 0) {
        battle_target_sort_list(g_battle_action_target->misc_unit_id);
        battle_action_clear_data();
        battle_formula_clear_nullify_flags();
        battle_action_clear_knockback_flag();
        g_current_ability.post_formula_flag = 0;
        battle_formula_force_attack_miss();
        g_battle_action_target_data->miss_type = BATTLE_ACTION_MISS_TYPE_CANCELLED;
        return;
    }
    if ((s16)g_battle_action_target_data->attack_accuracy == 0) {
        g_battle_action_target_data->hit = 0;
    }
    if ((s16)g_battle_action_target_data->hp_damage >= 1000) {
        g_battle_action_target_data->hp_damage = 999;
    }
    if ((s16)g_battle_action_target_data->hp_healing >= 1000) {
        g_battle_action_target_data->hp_healing = 999;
    }
    if ((s16)g_battle_action_target_data->mp_damage >= 1000) {
        g_battle_action_target_data->mp_damage = 999;
    }
    if ((s16)g_battle_action_target_data->mp_healing >= 1000) {
        g_battle_action_target_data->mp_healing = 999;
    }
    if ((s16)g_battle_action_target_data->hp_damage >= g_battle_action_target->hp) {
        battle_action_clear_knockback_flag();
        battle_formula_clear_nullify_flags();
    }
    team = (g_battle_action_target->initial_team_flags & BATTLE_TEAM_MASK) >> 4;
    action = g_battle_action_target_data;
    if ((action->attack_type != 0) && (action->hit != 0) && (g_battle_action_context == BATTLE_ACTION_CONTEXT_PRIMARY)
        && (g_battle_team_golem[team] != 0)
        && (g_current_ability.range_data.flags_3 & ABILITY_SECONDARY_FLAG_3_BLOCKED_BY_GOLEM)) {
        action->miss_type = BATTLE_ACTION_MISS_TYPE_GOLEM_GUARD;
        action->special_effect = BATTLE_ACTION_SPECIAL_EFFECT_GOLEM_GUARD;
        battle_action_clear_status_changes(g_battle_action_target_data);
        battle_action_clear_knockback_flag();
    }
}
