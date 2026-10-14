#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_target.h"

/* Enter the menu-to-targeting state and restart the source unit's command. */
void battle_state_restart_menu_to_targeting(void) {
    battle_unit_t* unit;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MENU_TO_TARGETING;
    unit = battle_unit_get_source();
    unit->state_frame_counter = 0;
    unit->command_ready = 0;
    target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 0);
    if (unit != 0 && unit->stats != 0) {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, unit->stats->battle_id, 0);
    }
}
