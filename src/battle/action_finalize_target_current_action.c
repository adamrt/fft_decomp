#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/battle_target.h"
#include "psx/types.h"

/* Validate and clamp the target's current action result before it applies.
 *
 * A target that is dead (unless the ability cancels Dead or the target is
 * undead), petrified or walled, without the ability canceling that status,
 * turns the action into a miss. Otherwise zero accuracy clears the hit flag,
 * HP/MP changes are capped at 999, a lethal HP hit drops knockback, and a
 * hit absorbed by the target team's Golem becomes a guarded miss. */
void action_finalize_target_current_action(void) {
    action_data_t* action;
    u8 status;
    s32 invalid;
    s32 team;

    status = g_action_target->status_sets.current[0];
    invalid = 0;
    if ((status & STATUS_BYTE_MASK(STATUS_ID_DEAD))
        && (!(g_action_canceled_statuses[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
            || (status & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)))) {
        invalid = 1;
    }
    if ((g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
            & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
        && !(g_action_canceled_statuses[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))) {
        invalid = 1;
    }
    if ((g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_WALL)] & STATUS_BYTE_MASK(STATUS_ID_WALL))
        && !(g_action_canceled_statuses[STATUS_BYTE_INDEX(STATUS_ID_WALL)] & STATUS_BYTE_MASK(STATUS_ID_WALL))) {
        invalid = 1;
    }
    if (invalid != 0) {
        target_sort_list(g_action_target->battle_id);
        action_clear_data();
        formula_clear_nullify_flags();
        action_clear_knockback_flag();
        g_action_current_ability.post_action_message = BATTLE_POST_ACTION_MESSAGE_NONE;
        formula_force_attack_miss();
        g_action_target_data->miss_type = ACTION_MISS_TYPE_CANCELED;
        return;
    }
    if (g_action_target_data->attack_accuracy == 0) {
        g_action_target_data->hit = 0;
    }
    if (g_action_target_data->hp_damage >= 1000) {
        g_action_target_data->hp_damage = 999;
    }
    if (g_action_target_data->hp_healing >= 1000) {
        g_action_target_data->hp_healing = 999;
    }
    if (g_action_target_data->mp_damage >= 1000) {
        g_action_target_data->mp_damage = 999;
    }
    if (g_action_target_data->mp_healing >= 1000) {
        g_action_target_data->mp_healing = 999;
    }
    if (g_action_target_data->hp_damage >= g_action_target->hp) {
        action_clear_knockback_flag();
        formula_clear_nullify_flags();
    }
    team = (g_action_target->initial_team_flags & BATTLE_TEAM_MASK) >> BATTLE_TEAM_SHIFT;
    action = g_action_target_data;
    if ((action->attack_type != 0) && (action->hit != 0) && (g_action_context == ACTION_CONTEXT_PRIMARY)
        && (g_battle_team_golem[team] != 0)
        && (g_action_current_ability.range_data.flags_3 & ABILITY_SECONDARY_FLAG_3_BLOCKED_BY_GOLEM)) {
        action->miss_type = ACTION_MISS_TYPE_GOLEM_GUARD;
        action->special_effect = ACTION_SPECIAL_EFFECT_GOLEM_GUARD;
        action_clear_status_changes(g_action_target_data);
        action_clear_knockback_flag();
    }
}
