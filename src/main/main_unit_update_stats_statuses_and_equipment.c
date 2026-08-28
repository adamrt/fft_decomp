#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "psx/types.h"

/* Re-derives the job-dependent stats (move/jump, equippable categories,
 * innate/immune/starting statuses, elemental affinity, R/S/M flags and
 * equipment bonuses), then reconciles the status bits.  `skip_status_check`
 * leaves the critical/status notifications alone; `initializing` seeds the
 * inflicted statuses from the current ones and clears their CTs. */
void main_unit_update_stats_statuses_and_equipment(battle_stats_t* unit, s32 skip_status_check, s32 initializing) {
    u8 saved_inflicted[BATTLE_STATUS_BYTE_COUNT];
    u8 saved_current[BATTLE_STATUS_BYTE_COUNT];
    s32 i;
    s32 status_id;
    s32 mask;
    s32 byte_index;
    s32 current_bit;
    s32 misc_unit_id;
    s32 saved_bit;
    s32 enabled;
    u8 formation_index;
    job_data_t* job;

    misc_unit_id = unit->misc_unit_id;
    main_status_store_current(unit);
    for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
        saved_current[i] = unit->status_sets.current[i];
        saved_inflicted[i] = unit->inflicted_status[i];
    }
    job = &g_job_data_pointer[unit->job_id];
    unit->move = job->move;
    unit->jump = job->jump & 0x7F;
    main_util_copy_bytes(job->equipment_categories, unit->equipment_categories, 4);
    main_util_copy_bytes(&job->status_sets, &unit->status_sets, 15);
    main_util_copy_bytes(job->elemental_affinity, unit->elemental_affinity, 4);
    unit->elemental_affinity[ELEMENTAL_AFFINITY_STRENGTHEN] = 0;
    main_unit_enable_rsm_flags(unit);
    main_unit_calculate_move_jump(unit, 1);
    main_unit_set_equipment_stats(unit);
    main_unit_set_equipment_attributes(unit, 0);
    main_unit_set_equippable_items(unit);

    for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
        if (initializing) {
            unit->inflicted_status[i] = unit->status_sets.current[i];
        } else {
            unit->inflicted_status[i] = saved_inflicted[i];
        }
    }

    formation_index = unit->formation_index;
    unit->formation_index = BATTLE_FORMATION_INDEX_NONE;
    main_status_update_unit_flags_and_ct(unit);
    unit->formation_index = formation_index;
    main_status_store_current(unit);

    if (skip_status_check != 0) {
        return;
    }
    if ((u16)(unit->max_hp / 5) >= unit->hp) {
        unit->inflicted_status[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CRITICAL)]
            |= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRITICAL);
    } else {
        unit->inflicted_status[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CRITICAL)]
            &= ~BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRITICAL);
    }
    main_status_store_current(unit);

    i = 0;
    status_id = BATTLE_STATUS_HANDLER_INDEX(BATTLE_STATUS_ID_UNNAMED_00);
    do {
        byte_index = i / 8;
        mask = 0x80 >> (i & 7);
        saved_bit = saved_current[byte_index] & mask;
        current_bit = unit->status_sets.current[byte_index] & mask;
        if (initializing != 0 || saved_bit != current_bit) {
            if (current_bit != 0) {
                if (initializing != 0) {
                    main_status_set_ct(unit, i, 0);
                }
                battle_status_enable_special_flags(status_id, 1, misc_unit_id);
            } else {
                battle_status_enable_special_flags(status_id, 0, misc_unit_id);
            }
        }
        i++;
        status_id++;
    } while (i < BATTLE_STATUS_COUNT);
}
