#include "fft/battle.h"
#include "psx/types.h"

void battle_state_enter_unit_moving(void) {
    battle_unit_misc_data_t* unit;

    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_UNIT_MOVING;
    battle_target_store_cursor_unit_name_and_data();
    unit = battle_unit_get_casting_misc_data();
    battle_unit_save_previous_state(unit);
    if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_MOUNT) {
        battle_unit_misc_data_t* mount = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
        if (mount != 0) {
            battle_unit_save_previous_state(mount);
        }
    }
    unit->step_speed = 0x2000;
    unit->movement_path_offset = 0;
    g_battle_state_animation_continue_check = 0;
    battle_action_clear_at_list_active();
}
