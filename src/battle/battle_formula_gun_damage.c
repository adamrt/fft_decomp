#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_gun_damage(void) {
    u16 weapon_power = g_current_ability.weapon_data.power;

    g_current_ability.xa = weapon_power;
    g_current_ability.ya = weapon_power;
    battle_formula_apply_charge();
    battle_formula_calculate_physical_damage();
}
