#include "fft/battle.h"
#include "psx/types.h"

void battle_state_enter_action_cast(void) {
    battle_unit_misc_data_t* unit;

    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_ACTION_CAST;
    g_battle_action_post_action = 0;
    unit = battle_unit_get_source_misc_data();
    if (battle_menu_init_action_menu(unit) == 0) {
        g_battle_game_state = BATTLE_GAME_STATE_ACTION_CAST;
        battle_camera_rotate_when_unit_tile_not_visible(unit);
    }
}
