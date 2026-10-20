/* Yielding the current thread forever. */

#include "fft/thread.h"

/* Park this event thread while allowing the scheduler to continue. */
void thread_wait_forever(void) {
    while (1) {
        thread_yield();
    }
}
