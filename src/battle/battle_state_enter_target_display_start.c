#include "fft/battle.h"
#include "fft/battle_camera.h"

/* Enter the target-display-start state: show the tile info panel and open the
 * targeting prompt for the casting unit. */
void battle_state_enter_target_display_start(void) {
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_DISPLAY_START;
    menu_start_unit_view_thread(UNIT_VIEW_MODE_TILE_INFO, 0xFF, 0xFF);
    menu_init_system_function(
        BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 0, battle_unit_get_casting()->stats->battle_id, 0, 1);
    g_action_post_action = 0;
    action_set_at_list_active();
}
