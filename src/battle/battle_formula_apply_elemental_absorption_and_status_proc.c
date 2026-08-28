#include "fft/battle.h"

s32 battle_formula_apply_elemental_absorption_and_status_proc(void) {
    battle_formula_apply_elemental_absorption();
    return battle_formula_roll_conditional_status_proc();
}
