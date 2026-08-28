#include "fft/battle.h"

void battle_menu_set_next_script_action_menus(void) {
    battle_unit_misc_data_t* casting_unit;
    battle_unit_misc_data_t* unit;

    battle_state_stop_game_flow();
    casting_unit = battle_unit_get_casting_misc_data();
    unit = battle_unit_get_source_misc_data();
    battle_action_set_at_list_active();
    battle_unit_store_entd_flags_without_control_flag(casting_unit);
    if (battle_menu_init_action_menu(unit) == 0) {
        g_battle_game_state = BATTLE_GAME_STATE_OPEN_ACTION_MENUS;
        g_battle_action_post_action = 0;
        battle_camera_rotate_when_unit_tile_not_visible(casting_unit);
    }
}
