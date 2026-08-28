#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_store_pa_and_weapon_power(void) {
    s32 pa = g_battle_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    s32 weapon_power = g_current_ability.weapon_data.power;

    g_current_ability.ya = weapon_power;
    g_current_ability.xa = pa;
}
