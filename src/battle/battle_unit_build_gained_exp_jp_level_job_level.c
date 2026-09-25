#include "fft/battle.h"
#include "psx/types.h"

/* Simulate the unit's pending action and write the EXP and JP gained and any
 * new level and job level to rewards[0..3]. Returns -1 for an empty unit
 * slot, otherwise the action finalization result. */
s32 battle_unit_build_gained_exp_jp_level_job_level(battle_stats_t* unit, battle_action_reward_display_t* rewards) {
    s32 job_slot[2];
    s32 result;
    u8* level_gained;
    s32 unit_id;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    unit_id = unit->misc_unit_id;
    level_gained = &g_current_ability.level_gained_flag;
    *level_gained = 0;
    g_current_ability.job_level_gained_flag = 0;
    g_battle_action_context = BATTLE_ACTION_CONTEXT_REACTION_OR_SIMULATION;
    g_battle_acting_unit_id = unit_id;
    result = battle_action_finalize_attack_and_flag_reactions(unit->misc_unit_id);
    g_battle_action_context = BATTLE_ACTION_CONTEXT_PRIMARY;
    rewards->earned_experience = unit->action.exp_change;
    rewards->earned_jp = unit->action.jp_change;
    rewards->level_for_display = 0;
    if (*level_gained != 0) {
        rewards->level_for_display = unit->level;
    }
    rewards->job_level_for_display = 0;
    if (g_current_ability.job_level_gained_flag != 0) {
        rewards->job_level_for_display = battle_unit_load_job_level(unit, unit->job_id, job_slot);
    }
    return result;
}
