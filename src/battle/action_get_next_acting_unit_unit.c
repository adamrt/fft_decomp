#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Enter the active-turn state and show entry at_index of the AT list.
 *
 * For a charged action (bit 0x100 of the entry) the cursor moves to the action's
 * target and the tiles it will hit are marked; for a unit's turn the cursor moves
 * to the unit and the tile marks are removed. An empty entry leaves the cursor
 * alone. */
void action_get_next_acting_unit(s32 at_index) {
    s32 id;
    battle_unit_t* unit;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ACTIVE_TURN;
    id = action_calculate_at_list_and_get_specific_unit_id(at_index);
    if (id >= 0) {
        unit = battle_unit_get_by_battle_id(id & 0xFF);
        if ((id & 0x100) != 0) {
            target_move_cursor_to_action_target(unit);
            target_calculate_for_menu_types(&unit->stats->action_command);
            target_set_tile_background_color(BATTLE_TILE_MARK_SHOW_TARGETS, 3);
        } else {
            target_move_cursor_to_unit(unit);
            target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 0);
        }
    }
    target_store_cursor_unit_name_and_data();
    action_set_at_list_active();
}

/* Enter the unit-move state and show the source unit's targeting prompt. */
void action_handle_move_command(void) {
    battle_unit_t* unit;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_UNIT_MOVE;
    unit = battle_unit_get_source();
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 0, unit->stats->battle_id, 0,
        unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Enter the move-range-exception state, reached when the casting unit has no
 * reachable tile: opens targeting-prompt option 2 for the source unit. */
void battle_state_enter_move_range_exception(void) {
    battle_unit_t* unit;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MOVE_RANGE_EXCEPTION;
    unit = battle_unit_get_source();
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 2, unit->stats->battle_id, 0,
        unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
}

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
