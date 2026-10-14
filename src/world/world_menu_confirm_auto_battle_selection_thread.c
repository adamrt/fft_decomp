#include "fft/thread.h"
#include "fft/world.h"

/* Applies the auto-battle choice of the menu selection (3 or 4) to the stored unit and confirms the
 * thread's menu entry without a sound. */
void world_menu_confirm_auto_battle_selection_thread(void) {
    menu_entry_t* entry = (menu_entry_t*)world_thread_get_current_parameter_1();
    unit_stats_t* unit = world_unit_get_active_stats();
    menu_entry_t* menu = g_menu_thread_menu_data;

    if (menu[1].selected_index == 3) {
        unit->auto_battle_setting = AI_AUTOBATTLE_FADING_LIFE;
    } else if (menu[1].selected_index == 4) {
        unit->auto_battle_setting = AI_AUTOBATTLE_RETREAT;
    }
    g_world_menu_set_auto_battle_row_actions = -3;
    menu_confirm_action_silently(entry);
    thread_exit_current();
}
