#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_apply_charge(void) {
    u16* xa = &g_current_ability.xa;

    *xa += g_current_ability.charge_power;
}
