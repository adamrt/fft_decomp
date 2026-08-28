#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x3A: +Brave (Y). */
void battle_formula_add_brave_y(void) {
    g_battle_action_target_data->brave_change = g_current_ability.range_data.y | BATTLE_ACTION_STAT_CHANGE_INCREASE;
    g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_PSEUDO_STATUS;
}
