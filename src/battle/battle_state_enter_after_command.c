#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_target.h"

/* Enter the after-command state at animation speed 1: put the cursor on the
 * source unit and close the open system window. */
void battle_state_enter_after_command(void) {
    g_main_system_animation_speed = 1;
    g_battle_game_state = BATTLE_GAME_STATE_AFTER_COMMAND;
    g_action_post_action = 0;
    target_move_cursor_to_unit(battle_unit_get_source());
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_CLOSE, 0, 0, 0, 0);
}
