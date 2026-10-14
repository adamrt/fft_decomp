#include "fft/battle_unit.h"
#include "fft/world.h"
#include "psx/types.h"

/* Run a system function for unit_id, looking up the unit's command action
 * first (except for commands 6 and 8); command 5 goes to the spell-quote
 * handler instead of menu_dispatch_system_function. */
void world_menu_init_system_function(s32 command, s32 option, s32 unit_id, s32 parameter, s32 enabled) {
    action_command_t* action;

    if (command != BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK && command != BATTLE_MENU_SYSTEM_COMMAND_CLOSE) {
        action = battle_unit_get_runtime_action_command_by_battle_id(unit_id);
        if (command == BATTLE_MENU_SYSTEM_COMMAND_SPELL_QUOTE) {
            text_select_ability_announcement(action, unit_id, enabled);
            return;
        }
        if (command == BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT) {
            menu_dispatch_system_function(BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT, 0, unit_id, 0, 0, action);
            return;
        }
    }
    menu_dispatch_system_function(command, option, unit_id, parameter, enabled, action);
}
