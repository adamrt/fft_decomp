#include "fft/battle.h"
#include "fft/battle_camera.h"

/* Enter the ability-preview-help state and start the help menu. Nothing in the
 * game calls it. */
void battle_menu_start_help_thread(void) {
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ABILITY_PREVIEW_HELP;
    g_battle_menu_help_opening = 1;
    menu_start_building_thread();
}
