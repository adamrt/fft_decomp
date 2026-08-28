#include "fft/job.h"
#include "fft/main_unit.h"
#include "psx/types.h"

/* Adds JP to a battle unit's job slot and recomputes its level and unlocks.
 *
 * Skips absent, crystal, dead, treasure, monster and other-team units, and
 * the Bard slot for females and the Dancer slot for males. The battle twin of
 * main_job_add_proposition_jp; returns -1 when nothing is stored. */
s32 battle_unit_store_jp_and_calculate_unlocked_jobs(s32 unit_index, s32 jp, s32 team, s32 slot) {
    battle_stats_t* unit;
    s32 total;
    s32 level;
    s32 levels;
    u8 flags;
    u8 level_byte;

    if (jp < 0) {
        return -1;
    }
    unit = &g_battle_unit_stats[unit_index];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    if (unit->status_sets.current[0]
        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD))) {
        return -1;
    }
    if (unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE)) {
        return -1;
    }
    flags = unit->unit_flags;
    if (flags & UNIT_FLAG_MONSTER) {
        return -1;
    }
    if ((unit->initial_team_flags & BATTLE_TEAM_MASK) != (team & 0xff)) {
        return -1;
    }
    if (slot == JOB_ID_BARD - JOB_ID_SQUIRE && (flags & UNIT_FLAG_FEMALE)) {
        return -1;
    }
    if (slot == JOB_ID_DANCER - JOB_ID_SQUIRE && (flags & UNIT_FLAG_MALE)) {
        return -1;
    }
    total = unit->job_points[slot] + jp;
    if (total >= 10000) {
        total = 9999;
    }
    unit->job_points[slot] = total;
    total = unit->total_job_points[slot] + jp;
    if (total >= 10000) {
        total = 9999;
    }
    unit->total_job_points[slot] = total;
    level = main_job_calculate_level(total);
    levels = unit->job_levels[slot / 2];
    level_byte = level;
    if (slot & 1) {
        levels = level + (levels & 0xf0);
    } else {
        levels = (levels & 0xf) | (level_byte << 4);
    }
    unit->job_levels[slot / 2] = levels;
    main_job_store_unlock_bitset(unit->unlocked_jobs, main_job_calculate_unlocked(unit->job_levels, unit->unit_flags));
    return 0;
}
