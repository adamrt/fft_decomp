#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"

/* Enter the illegal-move state and show move feedback option 2, which rejects the
 * chosen destination. */
void battle_menu_open_illegal_move_help(void) {
    battle_unit_t* unit;
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ILLEGAL_MOVE_MENU;
    unit = battle_unit_get_source();
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, 2, unit->stats->battle_id, 0,
        unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
}
