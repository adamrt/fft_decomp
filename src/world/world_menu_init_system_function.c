#include "fft/world.h"
#include "psx/types.h"

extern u8* battle_unit_get_target_id_ptr_by_battle_id(u32 unit_id);

/* Run a system function for unit_id, looking up the unit's command action
 * first (except for commands 6 and 8); command 5 goes to the spell-quote
 * handler instead of world_menu_dispatch_system_function. */
void world_menu_init_system_function(s32 command, s32 option, s32 unit_id, s32 parameter, s32 enabled) {
    s32 action;

    if (command != 8 && command != 6) {
        action = (s32)battle_unit_get_target_id_ptr_by_battle_id(unit_id);
        if (command == 5) {
            world_text_determine_spell_quote((world_unit_command_action_t*)action, unit_id, enabled);
            return;
        }
        if (command == 7) {
            world_menu_dispatch_system_function(7, 0, unit_id, 0, 0, (struct battle_ai_command_action*)action);
            return;
        }
    }
    world_menu_dispatch_system_function(
        command, option, unit_id, parameter, enabled, (struct battle_ai_command_action*)action);
}
