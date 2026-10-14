#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"

/* Run a system function (battle_menu_system_command_e) for unit_id, looking up
 * the unit's command action first except for SCENARIO_CHECK and CLOSE.
 *
 * SPELL_QUOTE goes to text_select_ability_announcement, a void routine whose
 * leftover v0 is returned. ABILITY_ANNOUNCEMENT drops option, parameter and
 * enabled. Everything else returns menu_dispatch_system_function's result;
 * for SCENARIO_CHECK and CLOSE the action pointer passed on is unset. */
s32 menu_init_system_function(s32 type, s32 option, s32 unit_id, s32 parameter, s32 enabled) {
    action_command_t* action;

    if (type != BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK && type != BATTLE_MENU_SYSTEM_COMMAND_CLOSE) {
        action = battle_unit_get_runtime_action_command_by_battle_id(unit_id);
        if (type == BATTLE_MENU_SYSTEM_COMMAND_SPELL_QUOTE) {
            /* The target returns the void callee's leftover $v0. */
            return ((s32 (*)(action_command_t*, s32, s32))text_select_ability_announcement)(action, unit_id, enabled);
        }
        if (type == BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT) {
            return menu_dispatch_system_function(
                BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT, 0, unit_id, 0, 0, action);
        }
    }
    return menu_dispatch_system_function(type, option, unit_id, parameter, enabled, action);
}
