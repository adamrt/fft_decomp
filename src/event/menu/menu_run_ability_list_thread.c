/* An ability list thread that yields once and exits. */

#include "fft/thread.h"

/* ATTACK's stand-in for menu_run_ability_list_thread: yields once and exits. Nothing starts it. */
void menu_run_ability_list_thread(void) {
    thread_yield();
    thread_exit_current();
}
