/* Yielding the current thread forever. */

#include "fft/screen.h"
#include "fft/thread.h"

/* Yield to the next WORLD thread, forever. */
void screen_thread_yield_forever(void) {
    for (;;) {
        thread_yield();
    }
}
