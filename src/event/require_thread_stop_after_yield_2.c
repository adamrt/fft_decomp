#include "fft/event_require.h"

void require_thread_stop_after_yield_2(void) {
    battle_thread_yield();
    battle_thread_exit_current();
}
