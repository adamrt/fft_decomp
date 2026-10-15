#include "fft/main.h"
#include "psx/types.h"

/* Rebuild one runtime unit from its persistent party slot.
 *
 * use_world_text selects the name-text provider used by the shared final
 * setup. Returns -1 when the requested party slot is unavailable. */
s32 main_unit_init_job_data(unit_stats_t* unit, s32 slot, s32 use_world_text) {
    party_unit_t* party_unit;

    g_main_unit_name_uses_world_text = use_world_text;
    party_unit = main_party_get_unit(slot);
    if (party_unit == 0) {
        return -1;
    }
    if (party_unit->party_id == PARTY_ID_NONE) {
        return -1;
    }
    main_unit_init_job_levels(party_unit->career.job_points.total.values, party_unit->career.job_levels);
    main_job_store_unlock_bitset(party_unit->career.unlocked_jobs,
        main_job_calculate_unlocked(party_unit->career.job_levels, party_unit->gender_flags));
    main_unit_reset_battle_state(unit);
    unit->formation_index = slot;
    main_unit_init_battle_data(unit, party_unit);
    main_unit_copy_job_data(unit);
    main_unit_enable_rsm_flags(unit);
    main_unit_calculate_actual_stats(unit, 0);
    main_unit_set_equippable_items(unit);
    main_unit_apply_equipment_move_jump_and_name(unit);
    return 0;
}
