#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/* Collects the unit's learned Math Skill abilities (0x19e..0x1a5) whose
 * secondary-data flags intersect mask into out, -1-terminated, and returns
 * the count. */
s32 battle_menu_collect_math_skill_abilities_by_flags(s32 unit_id, u8 skillset, s16* out, u8 mask) {
    u16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 charge_times[0x18];
    u8 ability_flags[0x18];
    u8 turn_counts[0x18];
    s32 count;
    s32 ability_count;
    s32 i;
    u16 ability;
    u8* secondary_flags;
    u8 flag_bits;

    count = 0;
    if (battle_unit_get_existing_pointer(unit_id) == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_MATH_SKILL) {
        *out = -1;
        return 0;
    }
    ability_count = battle_menu_get_unit_skillset_ability_data(
        unit_id, SKILLSET_ID_MATH_SKILL, (s16*)abilities, mp_costs, charge_times, 0, ability_flags, turn_counts);
    if (ability_count == 0) {
        return 0;
    }
    for (i = 0; i < ability_count; i++) {
        ability = abilities[i];
        if ((u16)(ability - ABILITY_ID_MATH_FIRST) < CALCULATOR_ABILITY_COUNT) {
            secondary_flags = &g_main_math_rsm_ability_data_by_ability_id[(s16)ability];
            flag_bits = *secondary_flags;
            if (flag_bits & mask) {
                *out++ = ability;
                count++;
            }
        }
    }
    *out = -1;
    return count;
}
