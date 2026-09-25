#include "fft/battle.h"

/* Enter the menu-to-targeting state and restart the source unit's command. */
void battle_state_restart_menu_to_targeting(void) {
    battle_unit_misc_data_t* unit;

    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_MENU_TO_TARGETING;
    unit = battle_unit_get_source_misc_data();
    unit->state_frame_counter = 0;
    unit->command_ready = 0;
    battle_target_set_tile_background_color(0, 0);
    if (unit != 0 && unit->battle_data != 0) {
        battle_menu_store_unit_names_and_event_block_data(3, unit->battle_data->misc_unit_id, 0);
    }
}
