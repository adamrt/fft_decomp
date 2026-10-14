#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_target.h"

/* Open the Wait menu for the source unit, or skip it.
 *
 * A scenario check that finds a pending event whose finish operation is nonzero
 * starts that event instead. A unit whose turn status blocks the Wait menu goes
 * straight to the between-turn events. Otherwise the wait-menu state opens with
 * the cursor unit's panel. The AT-list-active flag follows whether the player
 * controls the unit. */
void action_choose_wait(void) {
    battle_unit_t* unit;

    camera_disable_map_pan();
    unit = battle_unit_get_source();
    if (menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 2, unit->stats->battle_id, 0, 1) == 2
        && script_get_event_finish_operation() != 0) {
        g_battle_previous_game_state = g_battle_game_state;
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 0, unit->stats->battle_id, 0, 1);
        action_set_casting_unit_id_ff();
        return;
    }
    if (status_calculate_turn_flags(unit->stats) & BATTLE_TURN_STATUS_BLOCKS_WAIT_MENU_MASK) {
        action_check_between_turn_events();
    } else {
        g_battle_game_state = BATTLE_GAME_STATE_WAIT_MENU;
        target_store_cursor_unit_name_and_data();
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_WAIT_MENU, 0, unit->stats->battle_id, 0,
            unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
    }
    if (unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
        action_set_at_list_active();
    } else {
        action_clear_at_list_active();
    }
}
