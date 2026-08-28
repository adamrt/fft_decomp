#include "fft/battle.h"

s32 battle_target_set_boxes_red(void) {
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_source_misc_data();
    unit->state_frame_counter = 0;
    switch (unit->ability_preview_phase) {
    case 0:
    case 1:
        battle_state_stop_game_flow();
        g_battle_game_state = BATTLE_GAME_STATE_TARGETING_RANGE;
        battle_target_set_tile_background_color(2, 2);
        battle_target_store_cursor_unit_as_preview_target();
        if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
            battle_action_set_at_list_active();
        } else {
            battle_action_clear_at_list_active();
        }
        break;
    case 2:
        battle_state_stop_game_flow();
        battle_target_select_tile();
        battle_target_store_cursor_unit_as_preview_target();
        break;
    case -1:
    case 3:
        battle_menu_dispatch_idle_action_menu();
        break;
    }
    return unit->ability_preview_phase;
}
