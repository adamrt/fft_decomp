#include "fft/battle.h"
#include "fft/option.h"

void battle_menu_open_active_unit_idle_action_menu(void) {
    battle_unit_misc_data_t* misc_data;

    battle_state_stop_game_flow();
    misc_data = battle_unit_get_source_misc_data();
    g_battle_casting_unit_id = misc_data->unit_id;
    battle_target_move_cursor_to_unit(misc_data);
    g_battle_game_state = BATTLE_GAME_STATE_IDLING_ACTION_MENUS;

    if (misc_data != 0 && misc_data->battle_data != 0) {
        battle_menu_store_unit_names_and_event_block_data(3, misc_data->battle_data->misc_unit_id, 0);
    }
    battle_menu_build_idle_action_menu(
        battle_menu_get_id_based_on_mount_moveable_actable(misc_data->battle_data->misc_unit_id));
}
