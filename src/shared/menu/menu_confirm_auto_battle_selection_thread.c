#include "fft/battle.h"
#include "fft/thread.h"

/* Confirm the current menu action, recording the auto-battle mode it selects. */
void menu_confirm_auto_battle_selection_thread(void) {
    unit_stats_t* attacker;
    s32 menu;
    menu_entry_t* parameter;

    parameter = (menu_entry_t*)battle_thread_get_current_parameter_1();
    attacker = menu_get_selected_unit_stats();
    menu = g_menu_thread_menu_data[BATTLE_MENU_ID_AUTO_BATTLE].selected_index;
    if (menu == 3) {
        attacker->auto_battle_setting = AI_AUTOBATTLE_FADING_LIFE;
    } else if (menu == 4) {
        attacker->auto_battle_setting = AI_AUTOBATTLE_RETREAT;
    }
    g_menu_set_auto_battle_row_actions = -3;
    menu_confirm_action_silently(parameter);
    thread_exit_current();
}
