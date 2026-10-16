/* An ability list thread that yields once and exits. */

#include "fft/thread.h"
#include "fft/unit_panel.h"

/* ATTACK's stand-in for unit_panel_ability_thread: yields once and exits. Nothing starts it. */
void unit_panel_ability_thread(void) {
    thread_yield();
    thread_exit_current();
}
