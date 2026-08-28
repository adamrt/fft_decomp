#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_dispatch_idle_action_menu(void) {
    battle_unit_misc_data_t* misc;

    misc = battle_unit_get_selectable_misc_data_at_map_coords(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (misc == 0) {
        return;
    }
    g_battle_casting_unit_id = misc->unit_id;
    if (g_battle_status_menu_open == 0 && misc->unit_id == g_battle_casting_misc_id) {
        if ((misc->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
            battle_menu_open_active_unit_idle_action_menu();
        } else {
            battle_state_restart_menu_to_targeting();
        }
    } else {
        battle_menu_open_non_active_unit_idle_action_menu();
    }
}
