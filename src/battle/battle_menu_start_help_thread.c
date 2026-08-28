#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_start_help_thread(void) {
    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_ABILITY_PREVIEW_HELP;
    g_battle_menu_help_opening = 1;
    battle_menu_start_building_thread();
}
