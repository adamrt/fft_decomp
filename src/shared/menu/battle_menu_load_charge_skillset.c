#include "fft/battle_menu.h"
#include "psx/types.h"

/* Loads the skillset's usable abilities with their CT and turn counts into the
 * caller's lists; the MP-cost and silence/reflect flag lists are scratch. */
s32 battle_menu_load_charge_skillset(s32 unit_id, s32 skillset, s16* abilities, u8* ability_ct, u8* turns) {
    u8 mp_costs[16];
    u8 ability_flags[16];
    return battle_menu_get_unit_skillset_ability_data(
        unit_id, skillset, abilities, mp_costs, ability_ct, 0, ability_flags, turns);
}
