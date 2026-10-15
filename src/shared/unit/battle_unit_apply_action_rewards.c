#include "fft/battle_action.h"
#include "fft/battle_unit.h"

/* Apply the acting unit's pending EXP and JP rewards.
 *
 * EXP saturates at 255 before the level-up check. JP is 8 + job level * 2 +
 * level / 4, from the level and job level before this award, or 0 when the
 * action earned no EXP; monsters earn none. Gained JP Up (* 3/2) affects only
 * the actor; other battle slots receive one quarter of the unmodified JP
 * through the callee's team filter. */
s32 battle_unit_apply_action_rewards(s32 unit_index, action_reward_display_t* display) {
    s32 job_slot;
    unit_stats_t* unit;
    u8 old_level;
    s32 experience;
    s32 jp;
    s32 unit_id;
    u8 team;
    u8 job_id;
    s32 old_job_level;
    s32 new_job_level;
    s32 job_component;
    s32 level_component;

    display->earned_experience = 0;
    display->earned_jp = 0;
    display->level_for_display = 0;
    display->job_level_for_display = 0;

    if (g_action_hamedo_flag != 0) {
        return -1;
    }
    if (g_action_current_ability.can_earn_exp_jp == 0) {
        return 0;
    }

    unit = &g_battle_unit_stats[unit_index];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD))) {
        return -1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
        & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        return -1;
    }

    experience = unit->experience + g_action_current_ability.earned_experience;
    old_level = unit->level;
    if (experience >= 0x100) {
        experience = 0xff;
    }
    unit->experience = experience;
    if (main_unit_check_level_up(unit) != 0 || g_action_current_ability.level_gained_flag != 0) {
        display->level_for_display = unit->level;
    }
    display->earned_experience = g_action_current_ability.earned_experience;

    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        return 0;
    }

    job_id = unit->job_id;
    old_job_level = battle_unit_load_job_level(unit, job_id, &job_slot);
    job_component = old_job_level * 2;
    level_component = (old_level >> 2) + 8;
    jp = job_component + level_component;
    if (g_action_current_ability.earned_experience == 0) {
        jp = 0;
    }

    team = unit->team_flags & BATTLE_TEAM_MASK;
    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        if (unit_id != unit_index) {
            battle_unit_store_jp_and_calculate_unlocked_jobs(unit_id, jp / 4, team, job_slot);
        }
    }

    if (unit->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_GAINED_JP_UP) {
        jp = jp * 3 / 2;
    }
    team = unit->initial_team_flags & BATTLE_TEAM_MASK;
    battle_unit_store_jp_and_calculate_unlocked_jobs(unit_index, jp, team, job_slot);

    new_job_level = battle_unit_load_job_level(unit, job_id, &job_slot);
    if (new_job_level != old_job_level) {
        display->job_level_for_display = new_job_level;
    }
    display->earned_jp = jp;
    return 0;
}
