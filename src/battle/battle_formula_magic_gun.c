#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_magic_gun(void) {
    u16 weapon_power;
    u16 ability_y;

    battle_formula_select_magic_gun_ability();
    weapon_power = g_current_ability.weapon_data.power;
    ability_y = g_current_ability.range_data.y;
    g_current_ability.xa = weapon_power;
    g_current_ability.ya = ability_y;
    battle_formula_apply_charge();
    battle_formula_apply_weapon_element_strengthen();
    battle_formula_apply_magical_support_status_compatibility();
    if (battle_formula_calculate_elemental_xa_times_ya() == 0) {
        battle_formula_calculate_faith();
        battle_formula_apply_elemental_absorption();
    }
}
