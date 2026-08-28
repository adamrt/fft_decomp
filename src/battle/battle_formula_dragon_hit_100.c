#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_dragon_hit_100(void) {
    battle_formula_check_dragon();
    if (g_battle_action_target_data->hit != 0) {
        battle_formula_apply_status_to_action();
    }
}
