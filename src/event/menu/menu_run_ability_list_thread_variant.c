#include "fft/thread.h"

/* ATTACK's stand-in for equip_panel_run_ability_list_thread: yields once and exits. Nothing starts it. */
void menu_run_ability_list_thread_variant(void) {
    thread_yield();
    thread_exit_current();
}
