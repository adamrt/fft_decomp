#include "fft/thread.h"

/* ATTACK's stand-in for menu_run_equipment_list_thread_variant: yields once and exits. Nothing starts it. */
void menu_run_equipment_list_thread_variant(void) {
    thread_yield();
    thread_exit_current();
}
