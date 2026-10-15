#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"

/* Return to choosing a move destination: recompute the source unit's reachable
 * tiles, show them and enter the close-move-help state with the cursor unit's
 * panel. With no reachable tile the move-range exception state is entered
 * instead. The AT-list-active flag follows whether the player controls the unit. */
void battle_menu_close_move_help(void) {
    battle_unit_t* unit;
    battle_unit_t* casting;

    camera_disable_map_pan();
    unit = battle_unit_get_source();
    casting = battle_unit_get_casting();
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        action_set_at_list_active();
    } else {
        action_clear_at_list_active();
    }

    if (move_set_reachable_tiles(casting->stats->battle_id, casting->map_x, casting->map_y, casting->map_z) > 0) {
        target_set_tile_background_color(BATTLE_TILE_MARK_SHOW_MOVE, 1);
        g_battle_game_state = BATTLE_GAME_STATE_CLOSE_MOVE_HELP;
        casting->state_frame_counter = 0;
        target_store_cursor_unit_name_and_data();
    } else {
        battle_state_enter_move_range_exception();
    }
}
