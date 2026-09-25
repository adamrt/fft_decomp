#include "fft/battle.h"
#include "fft/main.h"
#include "psx/types.h"

void main_unit_copy_job_data(battle_stats_t* unit) {
    job_data_t* job;
    s32 i;
    s32 flag_a;
    s32 flag_b;

    if (unit->character_identity < CHARACTER_IDENTITY_GENERIC_FIRST) {
        unit->base_job_skillset = g_job_data_pointer[unit->character_identity].skillset;
    } else {
        unit->base_job_skillset = 0;
    }

    job = g_job_data_pointer + unit->job_id;
    unit->primary_skillset = job->skillset;
    main_util_copy_byte_data(job->innate_abilities, unit->innate_abilities, 8);
    main_util_copy_byte_data(job->equipment_categories, unit->equipment_categories, 4);
    main_util_copy_byte_data(job->growths_multipliers, &unit->raw_stats[UNIT_RAW_STAT_DATA_BYTE_COUNT], 10);
    unit->move = job->move;
    unit->jump = job->jump & JOB_JUMP_VALUE_MASK;
    if (job->jump & JOB_JUMP_STEPPING_STONE) {
        unit->position.raw |= BATTLE_UNIT_POSITION_STEPPING_STONE;
    } else {
        unit->position.raw &= ~BATTLE_UNIT_POSITION_STEPPING_STONE;
    }
    main_util_copy_byte_data(&job->status_sets, &unit->status_sets, 15);

    flag_a = unit->team_flags & BATTLE_TEAM_FLAG_IMMORTAL;
    flag_b = unit->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION);
    if (flag_a != 0 && flag_b != 0) {
        u8* statuses;
        i = 0;
        statuses = unit->status_sets.immunity;
        for (; i < BATTLE_STATUS_BYTE_COUNT; i++) {
            if (flag_a != 0) {
                *statuses |= g_main_status_check_sets[MAIN_STATUS_CHECK_SET_IMMORTAL_IMMUNITY][i];
            }
            if (flag_b != 0) {
                *statuses |= g_main_status_check_sets[MAIN_STATUS_CHECK_SET_FORMATION_IMMUNITY][i];
            }
            statuses++;
        }
    }

    main_util_copy_byte_data(job->elemental_affinity, unit->elemental_affinity, 4);
    unit->elemental_affinity[ELEMENTAL_AFFINITY_STRENGTHEN] = 0;
    unit->spritesheet_id = job->spritesheet_id;
    unit->job_portrait_palette = job->job_portrait_palette;
    unit->graphic_variant = job->graphic_variant;
}
