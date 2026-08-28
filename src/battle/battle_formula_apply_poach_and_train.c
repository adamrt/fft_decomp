#include "fft/battle_ai.h"
#include "fft/data.h"
#include "fft/job.h"
#include "psx/types.h"

/* Resolve Secret Hunt and Train after an HP-damaging formula 01-06 hit.
 *
 * Only monsters (neither sex bit set) qualify, and not through a team Golem
 * while the ability can be blocked by it, nor when dead, petrified or
 * walled. A killing blow from a Secret Hunt attacker poaches a monster job
 * target; a hit leaving the target at or below 1/5 max HP lets a Train
 * attacker inflict Invite. */
void battle_formula_apply_poach_and_train(void) {
    battle_action_data_t* action;
    battle_stats_t* target;
    s32 remaining;

    action = g_battle_action_target_data;
    if (!(action->attack_type & BATTLE_ACTION_TYPE_HP_DAMAGE)) {
        return;
    }
    target = g_battle_action_target;
    remaining = target->hp - (s16)action->hp_damage + (s16)action->hp_healing;
    if (target->unit_flags & (UNIT_FLAG_FEMALE | UNIT_FLAG_MALE)) {
        return;
    }
    if ((g_battle_team_golem[(target->initial_team_flags & BATTLE_TEAM_MASK) >> 4] != 0)
        && (g_current_ability.range_data.flags_3 & ABILITY_SECONDARY_FLAG_3_BLOCKED_BY_GOLEM)) {
        return;
    }
    if (*(u16*)&target->status_sets.current[0]
        & (BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_DEAD) | BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_PETRIFY))) {
        return;
    }
    if (target->status_sets.current[3] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_WALL)) {
        return;
    }
    if (((remaining <= 0) || (action->status_infliction[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD)))
        && (g_battle_action_attacker->support_abilities[2] & BATTLE_SUPPORT_SET_3_SECRET_HUNT)
        && !(target->team_flags & BATTLE_TEAM_FLAG_IMMORTAL)) {
        if ((u32)(target->job_id - JOB_ID_MONSTER_FIRST) >= JOB_ID_MONSTER_COUNT) {
            return;
        }
        battle_action_clear_knockback_flag();
        battle_formula_clear_nullify_flags();
        g_battle_action_target_data->special_effect |= BATTLE_ACTION_SPECIAL_EFFECT_POACH;
        g_current_ability.post_formula_flag = 6;
        return;
    }
    if ((remaining > 0) && ((u16)(g_battle_action_target->max_hp / 5) >= remaining)
        && (g_battle_action_attacker->support_abilities[2] & BATTLE_SUPPORT_SET_3_TRAIN)) {
        g_battle_action_target_data->status_infliction[1] |= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_INVITE);
        if (battle_status_modify_inflictions(0) != 0) {
            g_battle_action_target_data->special_effect |= BATTLE_ACTION_SPECIAL_EFFECT_TEAM_CHANGE;
            g_battle_action_target_data->attack_type |= BATTLE_ACTION_TYPE_STATUS_CHANGE;
            g_current_ability.post_formula_flag = 9;
        }
    }
}
