#include "fft/battle.h"
#include "psx/types.h"

s32 battle_formula_apply_elemental_absorption_and_status(void) {
    s32 result;

    battle_formula_apply_elemental_absorption();
    result = battle_formula_roll_conditional_status_proc();
    if (result == 0) {
        battle_formula_apply_status();
        return 0;
    }
    return result;
}
