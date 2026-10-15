#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"

/* Open the action menu for the unit on the cursor tile, which becomes the casting
 * unit.
 *
 * The source unit, with the status menu closed, gets its own turn menu when the
 * player controls it and otherwise restarts the menu-to-targeting state. Any
 * other unit gets the non-active-unit menu. An empty tile does nothing. */
void battle_menu_dispatch_idle_action_menu(void) {
    battle_unit_t* unit;

    unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (unit == 0) {
        return;
    }
    g_battle_casting_runtime_id = unit->runtime_id;
    if (g_status_menu_open == 0 && unit->runtime_id == g_battle_unit_source_runtime_id) {
        if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
            battle_menu_open_active_unit_idle_action_menu();
        } else {
            battle_state_restart_menu_to_targeting();
        }
    } else {
        battle_menu_open_non_active_unit_idle_action_menu();
    }
}
