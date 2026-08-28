#include "fft/main_unit.h"
#include "psx/types.h"

/* Build one runtime unit from an ENTD definition.
 *
 * use_world_text selects the WORLD name provider during the shared final
 * setup. A temporary formation index enables level growth for generated units.
 * Mode 0x82 is the transformed-monster recalculation path and preserves the
 * existing runtime/ENTD-derived state. */
s32 main_unit_init_for_battle(battle_stats_t* unit, entd_unit_t* entd, u32 use_world_text, s32 kind) {
    g_main_unit_name_uses_world_text = use_world_text;
    /* Kind 0x82 skips the reset and the ENTD calculation. */
    if (kind != 0x82) {
        main_unit_reset_battle_state(unit);
        if (main_unit_calculate_entd_data(unit, entd) != 0) {
            return -1;
        }
    }
    if (kind == 0) {
        if ((entd->unit_flags & UNIT_FLAG_LOAD_FORMATION) != 0) {
            return 0;
        }
        if (entd->sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
            if (entd->birthday[0] == 0) {
                return 0;
            }
        }
    }
    main_unit_calculate_jobs_and_skillsets_from_entd(unit, entd);
    main_unit_calculate_abilities(unit, entd);
    main_unit_enable_rsm_flags(unit);
    main_unit_generate_raw_stats(unit);
    main_unit_copy_job_growths_and_multipliers(unit);
    if (unit->formation_index == BATTLE_FORMATION_INDEX_NONE) {
        unit->formation_index = BATTLE_FORMATION_INDEX_GENERATED_UNIT;
    }
    main_unit_calculate_actual_stats(unit, 0);
    if (unit->formation_index == BATTLE_FORMATION_INDEX_GENERATED_UNIT) {
        unit->formation_index = BATTLE_FORMATION_INDEX_NONE;
    }
    main_unit_set_equippable_items(unit);
    if (kind != 0x82) {
        if (unit->formation_index >= 0x14) {
            main_unit_calculate_entd_equipment(unit, entd);
        }
    }
    main_unit_apply_equipment_move_jump_and_name(unit);
    main_unit_store_ramza_name_birthday_zodiac(unit);
    return 0;
}
