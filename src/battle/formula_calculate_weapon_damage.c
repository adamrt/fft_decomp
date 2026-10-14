#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "psx/types.h"

/* Damage of formulas 0x01 and 0x02: XA and YA by weapon type, charge power,
 * weapon element strengthen, physical modifiers, critical roll, XA * YA, then
 * the target's affinity to the weapon's elements. Returns 0 when the added
 * effect applies (formula_roll_conditional_status_proc), 1 when it does
 * not or the element canceled the attack. */
s32 formula_calculate_weapon_damage(void) {
    formula_calculate_base_xa();
    formula_apply_charge();
    formula_apply_weapon_element_strengthen();
    formula_calculate_physical_damage();
    formula_modify_elemental_damage();
    if (g_action_target_data->hit == 0)
        return 1;
    formula_apply_elemental_absorption();
    return formula_roll_conditional_status_proc();
}
