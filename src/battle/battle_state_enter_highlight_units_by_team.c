#include "fft/battle.h"

void battle_state_enter_highlight_units_by_team(void) {
    battle_state_start_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_HIGHLIGHT_UNITS;
    g_controller_input_copy_12 = g_battle_controller_input;
    g_battle_controller_input = 2;
    battle_gfx_tint_all_units_by_team();
    battle_action_clear_at_list_active();
}
