#include "fft/job.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "psx/types.h"

/* 0x80066182: JP required to reach each job level, indexed by level 0..8
 * (entry 0 is 0; entries 1..8 are g_job_level_jp_requirements). */

/* Random JP for a job of the given level: the level JP requirement plus
 * rand(0..99), or 100 + rand(0..99) for level 0.
 * The integer address expression preserves the target's index-before-base
 * addu operands. Both table[level] and level + table reverse those operands
 * at +0x11c and +0x1c0 under the canonical compiler. */
#define RANDOM_JP_FOR_LEVEL(value, level)                                                                              \
    if ((level) != 0) {                                                                                                \
        s32 roll = rand() * 100;                                                                                       \
        u16* requirement = (u16*)((level) * 2 + (u32)table);                                                           \
        if (roll < 0) {                                                                                                \
            roll += 0x7fff;                                                                                            \
        }                                                                                                              \
        (value) = *requirement + (roll >> 15);                                                                         \
    } else {                                                                                                           \
        s32 roll = rand() * 100;                                                                                       \
        if (roll < 0) {                                                                                                \
            roll += 0x7fff;                                                                                            \
        }                                                                                                              \
        (value) = (roll >> 15) + 100;                                                                                  \
    }

void main_unit_calculate_abilities(battle_stats_t* unit_arg, entd_unit_t* entd) {
    battle_stats_t* unit = unit_arg;
    u8 job_id = unit->job_id;
    u8 gender = unit->unit_flags;
    u8 byte;
    s32 i;
    s32 level;
    s32 job_slot;
    register s32 final_pair __asm__("$2");
    job_data_t* job;

    if (entd == 0) {
        main_util_clear_byte_data(&unit->reaction_ability, 6);
        return;
    }

    byte = entd->primary_skillset;
    if (byte != 0xff && byte != 0) {
        unit->primary_skillset = byte;
    } else {
        unit->primary_skillset = g_job_data_pointer[job_id].skillset;
    }

    if (gender & (UNIT_FLAG_MALE | UNIT_FLAG_FEMALE)) {
        battle_stats_t* jp_unit;
        u16* table;
        main_util_clear_byte_data(unit->learned_abilities, UNIT_LEARNED_ABILITY_BYTE_COUNT);
        table = g_main_job_jp_requirements_by_level;
        i = 0;
        /* Preserve table setup before the short-lived bound base. The
         * ordinary jp_unit copy remains available for strength reduction. */
        __asm__ __volatile__("" : : "r"(table));
        {
            register battle_stats_t* bound_base __asm__("$20") = unit;
            __asm__ __volatile__("" : "=r"(bound_base) : "0"(bound_base), "r"(table));
            jp_unit = bound_base;
        }
        job_slot = 0;
        do {
            /* The pair comparison and JP value use the return register in
             * successive lifetimes; neither needs to survive a call. */
            register s32 value __asm__("$2");

            byte = unit->job_levels[i];
            level = byte >> 4;
            final_pair = UNIT_JOB_LEVEL_BYTE_COUNT - 1;
            if (i == final_pair && (gender & UNIT_FLAG_MALE)) {
                value = 0; /* Dancer is female only */
            } else {
                RANDOM_JP_FOR_LEVEL(value, level)
            }
            jp_unit->total_job_points[i * UNIT_JOBS_PER_LEVEL_BYTE] = value;
            jp_unit->job_points[i * UNIT_JOBS_PER_LEVEL_BYTE] = value;
            main_unit_learn_job_abilities(unit, job_slot + JOB_ID_SQUIRE, entd);

            level = byte & 0xf;
            if (i == UNIT_JOB_LEVEL_BYTE_COUNT - 2 && (gender & UNIT_FLAG_FEMALE)) {
                value = 0; /* Bard is male only */
            } else {
                RANDOM_JP_FOR_LEVEL(value, level)
            }
            jp_unit->total_job_points[i * UNIT_JOBS_PER_LEVEL_BYTE + 1] = value;
            jp_unit->job_points[i * UNIT_JOBS_PER_LEVEL_BYTE + 1] = value;
            final_pair = UNIT_JOB_LEVEL_BYTE_COUNT - 1;
            if (i == final_pair) {
                break; /* Mime's JP is initialized, but its learning call is skipped. */
            }
            main_unit_learn_job_abilities(unit, job_slot + JOB_ID_SQUIRE + 1, entd);
            i++;
            job_slot += UNIT_JOBS_PER_LEVEL_BYTE;
        } while (i < UNIT_JOB_LEVEL_BYTE_COUNT);

        if (entd->primary_skillset != 0 && entd->primary_skillset != SKILLSET_ID_NONE) {
            unit->learned_abilities[0] = 0xff;
            unit->learned_abilities[1] = 0xff;
            unit->learned_abilities[2] = 0xff;
        }
    }

    i = 0;
    do {
        s32 k = i * 2;

        job = &g_job_data_pointer[job_id];
        unit->innate_abilities[i] = job->innate_abilities[k] + (job->innate_abilities[k + 1] << 8);
        unit->equipment_categories[i] = job->equipment_categories[i];
        i++;
    } while (i < 4);

    unit->reaction_ability = main_unit_calculate_rsm(unit, entd->reaction_ability, RSM_ABILITY_FILTER_REACTION, entd);
    unit->support_ability = main_unit_calculate_rsm(unit, entd->support_ability, RSM_ABILITY_FILTER_SUPPORT, entd);
    unit->movement_ability = main_unit_calculate_rsm(unit, entd->movement_ability, RSM_ABILITY_FILTER_MOVEMENT, entd);
}
