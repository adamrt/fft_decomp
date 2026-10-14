#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"

/* Open the source unit's own action menu: the unit becomes the casting unit, the
 * cursor returns to it, the idling-action-menus state begins with the unit
 * summary panel, and the menu for its remaining move and action is built. */
void battle_menu_open_active_unit_idle_action_menu(void) {
    battle_unit_t* unit;

    camera_disable_map_pan();
    unit = battle_unit_get_source();
    g_battle_casting_runtime_id = unit->runtime_id;
    target_move_cursor_to_unit(unit);
    g_battle_game_state = BATTLE_GAME_STATE_IDLING_ACTION_MENUS;

    if (unit != 0 && unit->stats != 0) {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, unit->stats->battle_id, 0);
    }
    battle_menu_build_idle_action_menu(battle_menu_get_id_based_on_mount_moveable_actable(unit->stats->battle_id));
}
