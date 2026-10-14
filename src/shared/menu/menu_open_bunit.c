#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/thread.h"

/* Start the unit-list transition in a child slot and wait for it to finish. */
void menu_open_bunit(void) {
    menu_set_modal_open();
    thread_start(g_thread_current_id - 1, battle_menu_run_bunit_transition_thread);
    thread_wait_until_inactive(g_thread_current_id - 1);
    thread_exit_current();
}
