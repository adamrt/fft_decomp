#include "fft/job.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"

/* Initialize packed job levels, unlocked jobs, and ENTD skillset overrides. */

void main_unit_calculate_jobs_and_skillsets_from_entd(battle_stats_t* unit, const entd_unit_t* entd) {
    s32 i;
    u32 unlocked;
    u32 flags;
    u8 selected;
    s32 random_job;
    s32 raw_value;
    u8 levels;
    u32 job;

    main_util_clear_byte_data(unit->job_levels, UNIT_JOB_LEVEL_BYTE_COUNT);
    unlocked = 0;
    if (!entd) {
        main_unit_copy_job_data(unit);
        unit->secondary_skillset = 0;
        unit->unit_flags = UNIT_FLAG_MONSTER;
        main_job_store_unlock_bitset(unit->unlocked_jobs, 0);
    } else {
        flags = entd->unit_flags;
        if (!(flags & UNIT_FLAG_MONSTER)) {
            selected = entd->job_unlock;
            if (selected < UNIT_CAREER_JOB_COUNT) {
                levels = entd->job_level & UNIT_JOB_LEVEL_LOW_NIBBLE_MASK;
                if (!(selected & 1))
                    levels <<= 4;
                unit->job_levels[selected / UNIT_JOBS_PER_LEVEL_BYTE] = levels;
                i = 0;
                job = selected;
                for (; i < UNIT_JOB_LEVEL_BYTE_COUNT; i++) {
                    if (job)
                        levels = g_job_unlock_requirements[job - 1][i];
                    else {
                        levels = UNIT_JOB_LEVEL_ONE_LOW_NIBBLE;
                        if (i != 0)
                            levels = UNIT_JOB_LEVEL_ONE_BOTH_NIBBLES;
                    }
                    levels |= unit->job_levels[i];
                    if (!(levels & UNIT_JOB_LEVEL_LOW_NIBBLE_MASK))
                        levels |= UNIT_JOB_LEVEL_ONE_LOW_NIBBLE;
                    if (!(levels & UNIT_JOB_LEVEL_HIGH_NIBBLE_MASK))
                        levels |= UNIT_JOB_LEVEL_ONE_HIGH_NIBBLE;
                    unit->job_levels[i] = levels;
                }
                if (flags & UNIT_FLAG_FEMALE)
                    unit->job_levels[UNIT_JOB_LEVEL_BYTE_INDEX_CALCULATOR_BARD] &= UNIT_JOB_LEVEL_HIGH_NIBBLE_MASK;
                if (flags & UNIT_FLAG_MALE)
                    unit->job_levels[UNIT_JOB_LEVEL_BYTE_INDEX_DANCER_MIME] &= UNIT_JOB_LEVEL_LOW_NIBBLE_MASK;
                unlocked = main_job_calculate_unlocked(unit->job_levels, flags);
            }
        }
        main_job_store_unlock_bitset(unit->unlocked_jobs, unlocked);
        unit->job_id = entd->job_id;
        main_unit_copy_job_data(unit);
        selected = entd->primary_skillset;
        if (selected != 0xff && selected != 0) {
            unit->primary_skillset = selected;
            unit->ability_outcome = 0xff;
        }
        selected = entd->secondary_skillset;
        if (selected == 0xfe) {
            if (unit->unit_flags & UNIT_FLAG_MONSTER)
                unit->secondary_skillset = 0;
            else {
                raw_value = main_job_get_random_unlocked(unit);
                levels = raw_value;
                if (levels)
                    random_job = g_job_data_pointer[levels].skillset;
                else
                    random_job = raw_value & 0xff;
                /* Keep the wide return separate from the final skillset at
                 * this join; GCC otherwise coalesces them into one register. */
                __asm__("" : : "r"(raw_value));
                if (random_job == unit->primary_skillset)
                    random_job = 0;
                unit->secondary_skillset = random_job;
            }
        } else
            unit->secondary_skillset = selected;
    }
}
