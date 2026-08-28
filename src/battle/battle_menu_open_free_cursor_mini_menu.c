#include "fft/battle.h"

void battle_menu_open_free_cursor_mini_menu(void) {
    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_MINI_MENU;
    battle_menu_start_mini_menu_display_thread();
}
