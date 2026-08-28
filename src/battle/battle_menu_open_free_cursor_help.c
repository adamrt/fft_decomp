#include "fft/battle.h"

void battle_menu_open_free_cursor_help(void) {
    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_FREE_CURSOR_HELP;
    g_battle_menu_help_opening = 1;
    battle_menu_start_building_thread();
}
