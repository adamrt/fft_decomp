#include "fft/battle.h"
#include "psx/types.h"

s32 battle_action_can_unit_react_1(battle_stats_t* unit) {
    if (battle_action_can_unit_react(unit) == 0 && battle_action_check_reaction(unit) == 0) {
        return battle_formula_can_unit_evade(unit);
    }
}
