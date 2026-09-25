#include "fft/battle.h"

/* Apply the acting unit's pending EXP and JP rewards.
 *
 * Gained JP Up affects only the actor; other battle slots receive one quarter
 * of the unmodified JP through the callee's team filter.
 */
s32 battle_unit_apply_action_rewards(s32 unit_index, battle_action_reward_display_t* display) {
    /* Pinned and laundered below: the display pointer lives in $s4 (unpinned, it swaps with team's $s3). */
    register battle_action_reward_display_t* result __asm__("$20");
    s32 job_slot;
    battle_stats_t* unit;
    u8* earned_experience;
    u8 old_level;
    s32 experience;
    s32 jp;
    s32 unit_id;
    s32 team;
    u8 job_id;
    s32 old_job_level;
    s32 new_job_level;
    s32 job_component;
    s32 level_component;

    result = display;
    /* Stores go through $s4 rather than the incoming $a1. */
    __asm__("" : "=r"(result) : "0"(result));
    result->earned_experience = 0;
    result->earned_jp = 0;
    result->level_for_display = 0;
    result->job_level_for_display = 0;

    if (g_current_ability_hamedo_flag != 0) {
        return -1;
    }
    if (g_current_ability.can_earn_exp_jp == 0) {
        return 0;
    }

    unit = &g_battle_unit_stats[unit_index];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    if (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CRYSTAL)]
        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD))) {
        return -1;
    }
    if (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_PETRIFY)]
        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PETRIFY) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE))) {
        return -1;
    }

    experience = unit->experience + g_current_ability.earned_experience;
    old_level = unit->level;
    if (experience >= 0x100) {
        experience = 0xff;
    }
    unit->experience = experience;
    if (main_unit_check_level_up(unit) != 0 || g_current_ability.level_gained_flag != 0) {
        result->level_for_display = unit->level;
    }
    earned_experience = &g_current_ability.earned_experience;
    result->earned_experience = *earned_experience;

    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        return 0;
    }

    job_id = unit->job_id;
    old_job_level = battle_unit_load_job_level(unit, job_id, &job_slot);
    job_component = old_job_level * 2;
    level_component = (old_level >> 2) + 8;
    jp = job_component + level_component;
    if (*earned_experience == 0) {
        jp = 0;
    }

    team = unit->team_flags & BATTLE_TEAM_MASK;
    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        if (unit_id != unit_index) {
            battle_unit_store_jp_and_calculate_unlocked_jobs(unit_id, jp / 4, team & 0xff, job_slot);
        }
    }

    if (unit->support_abilities[1] & BATTLE_SUPPORT_SET_2_GAINED_JP_UP) {
        jp = jp * 3 / 2;
    }
    battle_unit_store_jp_and_calculate_unlocked_jobs(
        unit_index, jp, unit->initial_team_flags & BATTLE_TEAM_MASK, job_slot);

    new_job_level = battle_unit_load_job_level(unit, job_id, &job_slot);
    if (new_job_level != old_job_level) {
        result->job_level_for_display = new_job_level;
    }
    result->earned_jp = jp;
    return 0;
}
