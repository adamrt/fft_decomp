#include "fft/battle.h"

void battle_menu_open_mini_menu_help(void) {
    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_MINI_MENU_HELP;
    g_battle_menu_help_opening = 1;
    battle_menu_start_building_thread();
}
