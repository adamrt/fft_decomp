/* Job levels, battle state reset, status init and ability range/AoE. */

#include "fft/main.h"
#include "psx/types.h"

/* Compute the packed job levels from the JP totals: one byte per pair of jobs,
 * the first job's level in the high nibble. */
void main_unit_init_job_levels(const u16* unit_job_jp, u8* unit_job_levels) {
    s32 i;
    s32 level;

    for (i = 0; i < UNIT_JOB_LEVEL_BYTE_COUNT; i++) {
        level = main_job_calculate_level(unit_job_jp[i * UNIT_JOBS_PER_LEVEL_BYTE]) << 4;
        unit_job_levels[i] = level | main_job_calculate_level(unit_job_jp[i * UNIT_JOBS_PER_LEVEL_BYTE + 1]);
    }
}

void main_unit_reset_battle_state(unit_stats_t* unit) {
    unit->auto_battle_setting = AI_AUTOBATTLE_MANUAL;
    unit->auto_battle_target = 0;
    unit->ct = 0;
    unit->has_turn = 0;
    unit->ability_outcome = 0;
    unit->charged_ability_ct = 0xff;
    unit->mount_info = 0;
    unit->ko_count = 0;
}

/* Raise a Speed of 0 to 1, optionally clear the war trophy and bonus money, and
 * initialize the unit's status data (main_status_init_unit). */
void main_unit_init_status_and_rewards(unit_stats_t* unit, s32 clear_rewards) {
    if (unit->attributes[UNIT_ATTRIBUTE_SPEED] == 0) {
        unit->attributes[UNIT_ATTRIBUTE_SPEED] = 1;
    }
    if (clear_rewards != 0) {
        unit->war_trophy = ITEM_ID_NOTHING;
        unit->bonus_money_modifier = 0;
    }
    main_status_init_unit(unit);
}

/* The range byte of an ordinary ability's range record; 0 for an id from
 * ABILITY_ID_ITEM_FIRST up, which has no such record. */
u32 main_ability_get_range(u32 ability_id) {
    u16 index;

    index = ability_id;
    return index < ABILITY_ID_ITEM_FIRST ? g_main_ability_range_data[index].range : 0;
}

/* The area-of-effect byte of an ordinary ability's range record; 0 for an id from
 * ABILITY_ID_ITEM_FIRST up, which has no such record. */
u32 main_ability_get_aoe(u32 ability_id) {
    u16 index;

    index = ability_id;
    return index < ABILITY_ID_ITEM_FIRST ? g_main_ability_range_data[index].aoe : 0;
}
