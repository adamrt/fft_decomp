#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_menu.h"

void battle_menu_open_help(void) {
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ACTION_HELP_MENU;
    g_battle_menu_help_opening = 1;
    menu_start_building_thread();
}
