#include "fft/battle.h"
#include "fft/data.h"

/* Pick the evade-animation type: Throw Stone is hardcoded to type 6, Item and
 * Throw abilities use the item-toss type 0x10, and everything else looks the
 * type up in the per-weapon evade table. The target retains a redundant
 * Shuriken test before checking the complete Item/Throw span. */
void battle_effect_set_evade_type_data_item_and_throw_stone_hardcoding(battle_unit_misc_data_t* unit) {
    battle_effect_secondary_init_t secondary_init;
    u16 ability_id;
    u8 evade_type;
    battle_effect_build_secondary_init_from_action(unit, &secondary_init);
    ability_id = unit->used_ability_id;
    if (ability_id == ABILITY_ID_BASIC_SKILL_THROW_STONE) {
        evade_type = 6;
    } else {
        evade_type = 0x10;
        if ((ability_id != ABILITY_ID_THROW_SHURIKEN)
            && ((u32)(unit->used_ability_id - ABILITY_ID_ITEM_FIRST)
                >= (ABILITY_ID_JUMP_FIRST - ABILITY_ID_ITEM_FIRST))) {
            evade_type = (&g_battle_effect_weapon_type_extra_flags)[unit->equipped_weapon_type];
        }
    }
    battle_effect_init_projectile_secondary(evade_type, &secondary_init);
}
