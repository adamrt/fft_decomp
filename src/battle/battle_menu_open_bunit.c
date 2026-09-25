#include "fft/battle.h"

/* Open BUNIT.OUT through a child thread and wait for its transition to finish. */
void battle_menu_open_bunit(void) {
    battle_menu_set_option_menu_open();
    battle_thread_start(g_battle_current_thread_id - 1, battle_menu_run_bunit_transition_thread);
    battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
    battle_thread_exit_current();
}
