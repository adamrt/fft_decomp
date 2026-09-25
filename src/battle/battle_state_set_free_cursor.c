#include "fft/battle.h"

void battle_state_set_free_cursor(void) {
    battle_unit_misc_data_t* unit;

    g_animation_speed = 1;
    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_FREE_CURSOR;
    g_battle_menu_help_opening = 0;
    battle_target_store_cursor_unit_name_and_data();
    battle_action_set_at_list_active();
    unit = battle_unit_get_source_misc_data();
    if (unit != 0) {
        battle_unit_store_entd_flags_without_control_flag(unit);
    }
}
