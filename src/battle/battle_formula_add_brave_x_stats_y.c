#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x3B: +Brave (X), +PA/MA/Speed (Y). */
void battle_formula_add_brave_x_stats_y(void) {
    s32 y;

    g_battle_action_target_data->brave_change = g_current_ability.range_data.x | BATTLE_ACTION_STAT_CHANGE_INCREASE;
    y = g_current_ability.range_data.y | BATTLE_ACTION_STAT_CHANGE_INCREASE;
    g_battle_action_target_data->pa_change = y;
    g_battle_action_target_data->ma_change = y;
    g_battle_action_target_data->sp_change = y;
    g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_PSEUDO_STATUS;
}
