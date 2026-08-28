#include "fft/battle.h"
#include "psx/types.h"

s32 battle_menu_init_action_menu(battle_unit_misc_data_t* unit) {
    s32 result;

    if (g_action_type == BATTLE_TURN_EVENT_UNIT_READY || g_action_type == BATTLE_TURN_EVENT_NONE) {
        result = battle_menu_init_system_function(8, 0, unit->battle_data->misc_unit_id, 0, 1);
    } else if (g_action_type == BATTLE_TURN_EVENT_ABILITY_READY || g_action_type == BATTLE_TURN_EVENT_ACTION_RESULT) {
        result = battle_menu_init_system_function(8, 2, unit->battle_data->misc_unit_id, 0, 1);
        if (result == 2 && battle_script_get_event_finish_operation() != 0) {
            if (unit->spritesheet_id < BATTLE_SPRITESHEET_ID_RAMZA_END) {
                result = battle_menu_init_system_function(8, 1, unit->battle_data->misc_unit_id, 0, 1);
            } else {
                battle_menu_init_system_function(8, 0, unit->battle_data->misc_unit_id, 0, 1);
            }
        } else {
            result = battle_menu_init_system_function(8, 1, unit->battle_data->misc_unit_id, 0, 1);
        }
    } else {
        result = battle_menu_init_system_function(8, 1, unit->battle_data->misc_unit_id, 0, 1);
    }
    if (result == 2) {
        g_previous_battle_game_state = g_battle_game_state;
        battle_action_set_casting_unit_id_ff();
        return 1;
    }
    return 0;
}
