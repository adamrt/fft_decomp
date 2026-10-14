#include "fft/battle.h"

/* ActionHelpMenu game-state handler: when the help window has gone, reopen the
 * idle action menu, the active unit's when the casting unit is the source unit
 * and the non-active unit's otherwise. */
void battle_state_handle_action_help_menu_state(void) {
    battle_unit_t* unit;

    if (battle_menu_is_still_building() != 2) {
        g_battle_menu_help_opening = 0;
        unit = battle_unit_get_casting();
        if (unit != 0) {
            if (unit->runtime_id == g_battle_unit_source_runtime_id) {
                battle_menu_open_active_unit_idle_action_menu();
            } else {
                battle_menu_open_non_active_unit_idle_action_menu();
            }
        }
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
}
