/* Yielding the current thread forever. */

#include "fft/menu_screen.h"
#include "fft/thread.h"

/* Yield to the next WORLD thread, forever. */
void thread_wait_forever(void) {
    for (;;) {
        thread_yield();
    }
}
