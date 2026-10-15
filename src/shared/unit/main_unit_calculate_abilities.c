#include "fft/main.h"
#include "fft/memory.h"
#include "psx/types.h"

/* Fills a unit's ability data from its ENTD entry: primary skillset, random
 * JP and learned abilities for each generic job (humans only), the job's
 * innate abilities and equipment categories, and the R/S/M abilities.
 *
 * Random JP for a job level: g_main_job_jp_requirements_by_level[level] (the
 * JP needed to reach it; 0x80066182, entry 0 is 0) plus rand(0..99), or
 * 100 + rand(0..99) for level 0. */
void main_unit_calculate_abilities(unit_stats_t* unit, entd_unit_t* entd) {
    u8 job_id = unit->job_id;
    u8 gender = unit->unit_flags;
    u8 byte;
    s32 i;
    s32 level;
    u16 value;

    if (entd == 0) {
        mem_zero(&unit->abilities, sizeof(unit->abilities));
        return;
    }

    byte = entd->primary_skillset;
    if (byte != 0xff && byte != 0) {
        unit->primary_skillset = byte;
    } else {
        unit->primary_skillset = g_main_job_data_pointer[job_id].skillset;
    }

    if (gender & (UNIT_FLAG_MALE | UNIT_FLAG_FEMALE)) {
        mem_zero(unit->career.learned_abilities, UNIT_LEARNED_ABILITY_BYTE_COUNT);
        for (i = 0; i < UNIT_JOB_LEVEL_BYTE_COUNT; i++) {
            byte = unit->career.job_levels[i];
            level = byte >> 4;
            if (i == UNIT_JOB_LEVEL_BYTE_COUNT - 1 && (gender & UNIT_FLAG_MALE)) {
                value = 0; /* Dancer is female only */
            } else if (level != 0) {
                value = g_main_job_jp_requirements_by_level[level] + rand() * 100 / 0x8000;
            } else {
                value = rand() * 100 / 0x8000 + 100;
            }
            unit->career.job_points.total.values[i * 2] = value;
            unit->career.job_points.current.values[i * 2] = value;
            main_unit_learn_job_abilities(unit, i * 2 + JOB_ID_SQUIRE, entd);

            level = byte & 0xf;
            if (i == UNIT_JOB_LEVEL_BYTE_COUNT - 2 && (gender & UNIT_FLAG_FEMALE)) {
                value = 0; /* Bard is male only */
            } else if (level != 0) {
                value = g_main_job_jp_requirements_by_level[level] + rand() * 100 / 0x8000;
            } else {
                value = rand() * 100 / 0x8000 + 100;
            }
            unit->career.job_points.total.values[i * 2 + 1] = value;
            unit->career.job_points.current.values[i * 2 + 1] = value;
            if (i == UNIT_JOB_LEVEL_BYTE_COUNT - 1) {
                break; /* Mime's JP is initialized, but its learning call is skipped. */
            }
            main_unit_learn_job_abilities(unit, i * 2 + JOB_ID_SQUIRE + 1, entd);
        }

        if (entd->primary_skillset != 0 && entd->primary_skillset != SKILLSET_ID_NONE) {
            unit->career.learned_abilities[0][0] = 0xff;
            unit->career.learned_abilities[0][1] = 0xff;
            unit->career.learned_abilities[0][2] = 0xff;
        }
    }

    for (i = 0; i < 4; i++) {
        s32 k = i * 2;

        unit->innate_abilities[i] = g_main_job_data_pointer[job_id].innate_abilities[k]
            + (g_main_job_data_pointer[job_id].innate_abilities[k + 1] << 8);
        unit->equipment_categories[i] = g_main_job_data_pointer[job_id].equipment_categories[i];
    }

    unit->abilities.reaction
        = main_unit_calculate_rsm(unit, entd->abilities.reaction, RSM_ABILITY_FILTER_REACTION, entd);
    unit->abilities.support = main_unit_calculate_rsm(unit, entd->abilities.support, RSM_ABILITY_FILTER_SUPPORT, entd);
    unit->abilities.movement
        = main_unit_calculate_rsm(unit, entd->abilities.movement, RSM_ABILITY_FILTER_MOVEMENT, entd);
}
