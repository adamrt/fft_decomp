#include "fft/battle.h"

/* Confirm the current menu action, recording the auto-battle mode it selects. */
void battle_menu_confirm_auto_battle_selection_thread(void) {
    battle_stats_t* attacker;
    s32 menu;
    s32 parameter;

    parameter = battle_thread_get_current_parameter_1();
    attacker = battle_unit_get_attacker_data_pointer();
    menu = g_battle_menu_thread_menu_data[1].selected_index;
    if (menu == 3) {
        attacker->auto_battle_setting = 0x10;
    } else if (menu == 4) {
        attacker->auto_battle_setting = 0x11;
    }
    g_battle_menu_set_auto_battle_row_actions = -3;
    battle_menu_confirm_action_silently((void*)parameter);
    battle_thread_exit_current();
}
