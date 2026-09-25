#include "fft/event_option.h"
#include "psx/types.h"

/* Close the AT list's worker set and restore the parent option threads. */
void option_menu_close_at_list_threads(void) {
    g_option_menu_open = 1;
    g_option_menu_submenu_state = 2;

    battle_thread_suspend(g_battle_current_thread_id + 1);
    battle_thread_suspend(g_battle_current_thread_id + 2);
    battle_thread_suspend(g_battle_current_thread_id + 3);
    battle_thread_suspend(g_battle_current_thread_id + 4);

    while ((g_option_menu_transition_timer = g_option_menu_transition_duration + 100),
        g_option_menu_transition_finished == 0) {
        battle_thread_yield();
    }

    g_option_menu_transition_finished = 0;
    g_option_menu_transition_timer = 0;
    battle_menu_store_unit_names_and_event_block_data(0, 0xff, 0xff);

    battle_thread_wait_until_inactive(13);
    battle_thread_wait_until_inactive(12);
    battle_thread_wait_until_inactive(11);
    battle_thread_wait_until_inactive(10);

    g_menu_text_pointer = g_option_menu_at_list_text;
    battle_thread_resume(g_battle_current_thread_id + 1);
    battle_thread_resume(g_battle_current_thread_id + 2);
    battle_thread_resume(g_battle_current_thread_id + 3);
    battle_thread_resume(g_battle_current_thread_id + 4);

    g_option_menu_open = 0;
    g_option_menu_submenu_state = 0;
    battle_thread_exit_current();
}
