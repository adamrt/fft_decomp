#include "fft/battle.h"
#include "psx/types.h"

/* Four skillset IDs copied as one unaligned word. */
typedef struct {
    u8 ids[4];
} battle_menu_calculator_skillsets_t;

extern battle_menu_calculator_skillsets_t g_battle_menu_calculator_skillsets;

/*
 * Collects the unit's Calculator-eligible abilities from the four skillsets
 * at g_battle_menu_calculator_skillsets into out, -1-terminated, and returns the count. Only the Math
 * Skill skillset collects anything.
 *
 * The record pointer must be taken inside the inner loop: hoisting the table
 * base into a local before the loop swaps the two preheader address setups.
 */
s32 battle_menu_collect_calculator_abilities(s32 unit_id, u8 skillset, s16* out) {
    s16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 charge_times[0x18];
    u8 ability_flags[0x18];
    u8 turn_counts[0x18];
    battle_menu_calculator_skillsets_t skillsets;
    s32 count;
    s32 ability_count;
    s32 i;
    s32 j;
    s16 ability;
    ability_secondary_data_t* secondary;

    count = 0;
    skillsets = g_battle_menu_calculator_skillsets;
    if (battle_unit_get_existing_pointer(unit_id) == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_MATH_SKILL) {
        *out = -1;
        return 0;
    }
    for (i = 0; i < 4; i++) {
        ability_count = battle_menu_get_unit_skillset_ability_data(
            unit_id, skillsets.ids[i], abilities, mp_costs, charge_times, 1, ability_flags, turn_counts);
        if (ability_count != 0) {
            for (j = 0; j < ability_count; j++) {
                ability = abilities[j];
                secondary = &g_main_ability_range_data[ability];
                if (secondary->flags_3 & ABILITY_SECONDARY_FLAG_3_CALCULATOR_ELIGIBLE) {
                    *out++ = ability;
                    count++;
                }
            }
        }
    }
    *out = -1;
    return count;
}
