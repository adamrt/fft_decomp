#include "fft/event_debugchr.h"

void debugchr_thread_stop_after_yield_2(void) {
    battle_thread_yield();
    battle_thread_exit_current();
}
