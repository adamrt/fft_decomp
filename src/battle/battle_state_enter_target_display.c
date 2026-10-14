#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_target.h"

/* Enter the target-display state: move the cursor to the casting unit's
 * auto-battle target, turn the camera when its tile is hidden and show its
 * summary panel. */
void battle_state_enter_target_display(void) {
    unit_stats_t* cast;
    battle_unit_t* unit;
    unit_stats_t* target;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_DISPLAY;
    cast = battle_unit_get_casting()->stats;
    unit = battle_unit_get_by_battle_id(cast->auto_battle_target);
    target_move_cursor_to_unit(unit);
    camera_rotate_when_unit_tile_not_visible(unit);
    if (unit != 0) {
        target = unit->stats;
        if (target != 0) {
            menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, target->battle_id, 0);
        }
    }
}
