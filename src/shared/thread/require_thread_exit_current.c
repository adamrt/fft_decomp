#include "fft/event_require.h"
#include "fft/thread.h"

void require_thread_exit_current(void) {
    thread_exit_current();
}
