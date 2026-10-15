#include "fft/event_option.h"
#include "fft/thread.h"

/* Give other event work one turn before terminating this thread. */
void thread_stop_after_yield(void) {
    thread_yield();
    thread_exit_current();
}
