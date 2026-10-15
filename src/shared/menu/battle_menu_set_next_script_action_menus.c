#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"

/* Return to the source unit's action menus: set the AT-list-active flag, run the
 * scenario check, and unless it starts an event enter the open-action-menus state
 * and turn the camera when the casting unit's tile is hidden. */
void battle_menu_set_next_script_action_menus(void) {
    battle_unit_t* casting_unit;
    battle_unit_t* unit;

    camera_disable_map_pan();
    casting_unit = battle_unit_get_casting();
    unit = battle_unit_get_source();
    action_set_at_list_active();
    battle_unit_store_entd_flags_without_control_flag(casting_unit);
    if (battle_state_try_enter_scenario_event(unit) == 0) {
        g_battle_game_state = BATTLE_GAME_STATE_OPEN_ACTION_MENUS;
        g_action_post_action = 0;
        camera_rotate_when_unit_tile_not_visible(casting_unit);
    }
}
