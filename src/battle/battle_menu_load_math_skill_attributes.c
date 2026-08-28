#include "fft/battle.h"
#include "fft/data.h"
#include "psx/types.h"

/* Collects the unit's learned Math Skill attribute abilities (CT, Level, Exp,
 * Height) into out_ability_ids. */
s32 battle_menu_load_math_skill_attributes(s32 unit_id, s32 skillset, s16* out_ability_ids) {
    return battle_menu_collect_math_skill_abilities_by_flags(
        unit_id, skillset & 0xFF, out_ability_ids, CALCULATOR_ATTRIBUTE_MASK);
}
