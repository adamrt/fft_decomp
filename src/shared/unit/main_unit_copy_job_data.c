#include "fft/main.h"
#include "fft/memory.h"
#include "psx/types.h"

/* Load a battle unit's job-derived fields from its job record: skillsets, innate
 * abilities, equippable types, growths and multipliers, Move, Jump, the innate,
 * immunity and starting status sets, elemental affinities and graphics ids.
 *
 * base_job_skillset is the skillset of the job whose id equals a special
 * character's identity, and 0 for a generic unit. The job's stepping-stone bit
 * moves from the Jump byte to the unit's position byte. An immortal unit that
 * also has a save- or load-formation flag gains the immunity check sets 5 and 6;
 * the tests inside that loop repeat the outer condition. The strengthened
 * elements are cleared. */
void main_unit_copy_job_data(unit_stats_t* unit) {
    job_data_t* job;
    s32 i;
    s32 flag_a;
    s32 flag_b;

    if (unit->character_identity < CHARACTER_IDENTITY_GENERIC_FIRST) {
        unit->base_job_skillset = g_main_job_data_pointer[unit->character_identity].skillset;
    } else {
        unit->base_job_skillset = 0;
    }

    job = g_main_job_data_pointer + unit->job_id;
    unit->primary_skillset = job->skillset;
    mem_copy_byte_data_source_first(job->innate_abilities, unit->innate_abilities, sizeof(unit->innate_abilities));
    mem_copy_byte_data_source_first(
        job->equipment_categories, unit->equipment_categories, sizeof(unit->equipment_categories));
    mem_copy_byte_data_source_first(job->stat_growths, unit->stat_growths, sizeof(unit->stat_growths));
    unit->move = job->move;
    unit->jump = job->jump & JOB_JUMP_VALUE_MASK;
    if (job->jump & JOB_JUMP_STEPPING_STONE) {
        unit->position.stepping_stone = 1;
    } else {
        unit->position.stepping_stone = 0;
    }
    mem_copy_byte_data_source_first(&job->status_sets, &unit->status_sets, sizeof(job->status_sets));

    flag_a = unit->team_flags & BATTLE_TEAM_FLAG_IMMORTAL;
    flag_b = unit->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION);
    if (flag_a != 0 && flag_b != 0) {
        u8* statuses = unit->status_sets.immunity;
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if (flag_a != 0) {
                statuses[i] |= g_main_status_check_sets[MAIN_STATUS_CHECK_SET_IMMORTAL_IMMUNITY][i];
            }
            if (flag_b != 0) {
                statuses[i] |= g_main_status_check_sets[MAIN_STATUS_CHECK_SET_FORMATION_IMMUNITY][i];
            }
        }
    }

    mem_copy_byte_data_source_first(job->elemental_affinity, unit->elemental_affinity, sizeof(job->elemental_affinity));
    unit->elemental_affinity[ELEMENTAL_AFFINITY_STRENGTHEN] = 0;
    unit->spritesheet_id = job->spritesheet_id;
    unit->job_portrait_palette = job->job_portrait_palette;
    unit->monster_graphic = job->monster_graphic;
}
