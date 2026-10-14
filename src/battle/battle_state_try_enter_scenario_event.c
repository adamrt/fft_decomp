#include "fft/battle.h"
#include "fft/battle_script.h"
#include "psx/types.h"

/* Run the scenario check for a unit at a turn boundary; when it starts an event,
 * save the game state, enter the event state and return 1. Returns 0 otherwise.
 *
 * With a unit becoming ready, or no turn event, the check may start an event
 * directly (option 0). For a ready ability or an action result the check first
 * only queues the event (option 2), and the event is started only when its finish
 * operation is nonzero and the unit's spritesheet is not one of Ramza's. In every
 * other case, and for the remaining turn event types, the check only closes the
 * menu (option 1) and no event starts. */
s32 battle_state_try_enter_scenario_event(battle_unit_t* unit) {
    s32 result;

    if (g_action_turn_event_type == BATTLE_TURN_EVENT_UNIT_READY
        || g_action_turn_event_type == BATTLE_TURN_EVENT_NONE) {
        result = menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 0, unit->stats->battle_id, 0, 1);
    } else if (g_action_turn_event_type == BATTLE_TURN_EVENT_ABILITY_READY
        || g_action_turn_event_type == BATTLE_TURN_EVENT_ACTION_RESULT) {
        result = menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 2, unit->stats->battle_id, 0, 1);
        if (result == MENU_SYSTEM_DISPATCH_EVENT && script_get_event_finish_operation() != 0) {
            if (unit->spritesheet_id < BATTLE_SPRITESHEET_ID_RAMZA_END) {
                result = menu_init_system_function(
                    BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 1, unit->stats->battle_id, 0, 1);
            } else {
                menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 0, unit->stats->battle_id, 0, 1);
            }
        } else {
            result
                = menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 1, unit->stats->battle_id, 0, 1);
        }
    } else {
        result = menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 1, unit->stats->battle_id, 0, 1);
    }
    if (result == MENU_SYSTEM_DISPATCH_EVENT) {
        g_battle_previous_game_state = g_battle_game_state;
        action_set_casting_unit_id_ff();
        return 1;
    }
    return 0;
}
