#include "fft/event_option.h"

/* Give other event work one turn before terminating this thread. */
void option_thread_stop_after_yield(void) {
    battle_thread_yield();
    battle_thread_exit_current();
}
