#include "fft/battle_action.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Simulate the unit's pending action and write the EXP and JP gained and any
 * new level and job level to rewards[0..3]. Returns -1 for an empty unit
 * slot, otherwise the action finalization result. */
s32 battle_unit_build_gained_exp_jp_level_job_level(unit_stats_t* unit, action_reward_display_t* rewards) {
    s32 job_slot[2];
    s32 result;
    s32 unit_id;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    unit_id = unit->battle_id;
    g_action_current_ability.level_gained_flag = 0;
    g_action_current_ability.job_level_gained_flag = 0;
    g_action_context = ACTION_CONTEXT_REACTION_OR_SIMULATION;
    g_battle_acting_unit_id = unit_id;
    result = action_finalize_attack_and_flag_reactions(unit->battle_id);
    g_action_context = ACTION_CONTEXT_PRIMARY;
    rewards->earned_experience = unit->action.exp_change;
    rewards->earned_jp = unit->action.jp_change;
    rewards->level_for_display = 0;
    if (g_action_current_ability.level_gained_flag != 0) {
        rewards->level_for_display = unit->level;
    }
    rewards->job_level_for_display = 0;
    if (g_action_current_ability.job_level_gained_flag != 0) {
        rewards->job_level_for_display = battle_unit_load_job_level(unit, unit->job_id, job_slot);
    }
    return result;
}
