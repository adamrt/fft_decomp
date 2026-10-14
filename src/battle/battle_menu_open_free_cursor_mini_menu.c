#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_menu.h"

void battle_menu_open_free_cursor_mini_menu(void) {
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MINI_MENU;
    menu_start_mini_menu_display_thread();
}
