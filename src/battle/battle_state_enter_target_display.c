#include "fft/battle.h"
#include "fft/option.h"
#include "psx/types.h"

void battle_state_enter_target_display(void) {
    battle_stats_t* cast;
    battle_unit_misc_data_t* unit;
    battle_stats_t* target;

    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_DISPLAY;
    cast = battle_unit_get_casting_misc_data()->battle_data;
    unit = battle_unit_get_misc_data_by_battle_id(cast->auto_battle_target);
    battle_target_move_cursor_to_unit(unit);
    battle_camera_rotate_when_unit_tile_not_visible(unit);
    if (unit != 0) {
        target = unit->battle_data;
        if (target != 0) {
            battle_menu_store_unit_names_and_event_block_data(3, target->misc_unit_id, 0);
        }
    }
}
