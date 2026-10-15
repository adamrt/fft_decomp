#include "fft/battle.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/types.h"

/* Resolve a crystal or treasure pickup for the unit that moved onto its tile.
 *
 * A treasure gives one item (result 4). A crystal (result 2) offers a human
 * mover the abilities of a human crystal, job by job: the base job only when
 * both share a character identity, the others when the mover has the job
 * unlocked (never Bard for a woman or Dancer for a man). Each ability the
 * crystal knows and the mover does not is inherited when a roll of 0..99 is
 * below its learn rate; result bit 0 is set when at least one is.
 *
 * The two one-trip loops around the crystal result store emit no code; they
 * count that use of the CSE'd &g_main_crystal_pickup_result pseudo two loop levels deeper
 * (8 refs), so global-alloc ranks it above `mover` and gives it s1 as in the
 * target. */
crystal_pickup_result_t* battle_unit_generate_crystal_or_treasure(unit_stats_t* mover) {
    u8 gained[UNIT_LEARNED_ABILITY_BYTES_PER_JOB];
    u8 known[UNIT_LEARNED_ABILITY_BYTES_PER_JOB];
    u8 offered[UNIT_LEARNED_ABILITY_BYTES_PER_JOB];
    s32 learned_count;
    s32 mover_identity;
    s32 crystal_identity;
    s32 mover_flags;
    s32 crystal_flags;
    s32 unlocked_jobs_mask;
    unit_stats_t* crystal;
    s32 crystal_index;
    s32 job_id;
    s32 job_index;
    s32 j;
    s32 byte_index;
    s32 mask;
    s32 skillset;
    s32 ability;
    u16 ability_id;

    crystal_index = battle_unit_find_at_tile(mover->x, mover->position.y, mover->position.higher_elevation,
        BATTLE_UNIT_TILE_FILTER_CRYSTAL | BATTLE_UNIT_TILE_FILTER_TREASURE);
    if (crystal_index < 0) {
        g_main_crystal_pickup_result.result = -1;
        return &g_main_crystal_pickup_result;
    }
    g_main_crystal_pickup_result.result = 0;
    g_main_crystal_pickup_result.unit_index = crystal_index;
    crystal = &g_battle_unit_stats[crystal_index];
    if (crystal->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
        g_main_crystal_pickup_result.result = 4;
        g_main_crystal_pickup_result.treasure_item = battle_unit_generate_treasure(crystal);
    } else {
        do {
            do {
                g_main_crystal_pickup_result.result = 2;
            } while (0);
        } while (0);
        mover_flags = mover->unit_flags;
        crystal_flags = crystal->unit_flags;
        if (!(mover_flags & UNIT_FLAG_MONSTER) && !(crystal_flags & UNIT_FLAG_MONSTER)) {
            learned_count = 0;
            unlocked_jobs_mask = (mover->career.unlocked_jobs[0] << 16) + (mover->career.unlocked_jobs[1] << 8)
                + mover->career.unlocked_jobs[2];
            mover_identity = mover->character_identity;
            crystal_identity = crystal->character_identity;
            mem_zero(g_main_crystal_pickup_result.learned_abilities, 0x40);
            for (job_index = UNIT_CAREER_JOB_INDEX_BASE; job_index < UNIT_LEARNED_ABILITY_JOB_COUNT; job_index++) {
                for (j = 0; j < UNIT_LEARNED_ABILITY_BYTES_PER_JOB; j++) {
                    g_main_crystal_pickup_result.learned[job_index][j] = 0;
                    gained[j] = 0;
                    known[j] = mover->career.learned_abilities[job_index][j];
                    offered[j] = crystal->career.learned_abilities[job_index][j];
                }
                job_id = job_index + JOB_ID_GENERIC_FIRST;
                if (job_index == UNIT_CAREER_JOB_INDEX_BASE) {
                    if (mover_identity != crystal_identity) {
                        continue;
                    }
                    if (mover_identity < CHARACTER_IDENTITY_SELECTOR_FIRST) {
                        job_id = mover_identity;
                    }
                } else if (!(unlocked_jobs_mask & (UNIT_UNLOCKED_JOB_SQUIRE >> job_index))) {
                    continue;
                }
                if (job_index == UNIT_CAREER_JOB_INDEX_BARD && (mover_flags & UNIT_FLAG_FEMALE)) {
                    continue;
                }
                if (job_index == UNIT_CAREER_JOB_INDEX_DANCER && (mover_flags & UNIT_FLAG_MALE)) {
                    continue;
                }
                skillset = g_main_job_data_pointer[job_id].skillset;
                for (j = 0; j < 24; j++) {
                    byte_index = j / 8;
                    mask = 0x80 >> (j - byte_index * 8);
                    if (!(known[byte_index] & mask) && (offered[byte_index] & mask)) {
                        ability = main_ability_get_id_from_skillset(skillset, j);
                        ability_id = ability;
                        if (ability_id != 0
                            && main_util_roll_pass_fail(100, g_main_ability_data[ability_id].learn_rate) == 0) {
                            gained[byte_index] = mask | gained[byte_index];
                            learned_count++;
                            if (ability_id < ABILITY_ID_COUNT) {
                                g_main_crystal_pickup_result.learned_abilities[ability_id >> 3] = (1 << (ability & 7))
                                    | g_main_crystal_pickup_result.learned_abilities[ability_id >> 3];
                            }
                        }
                    }
                }
                for (j = 0; j < UNIT_LEARNED_ABILITY_BYTES_PER_JOB; j++) {
                    g_main_crystal_pickup_result.learned[job_index][j] = gained[j];
                }
            }
            if (learned_count != 0) {
                g_main_crystal_pickup_result.result |= 1;
            }
        }
    }
    return &g_main_crystal_pickup_result;
}
