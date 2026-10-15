#include "fft/thread.h"
#include "fft/world.h"

/* Yield to the next WORLD thread, forever. */
void world_thread_yield_forever(void) {
    for (;;) {
        thread_yield();
    }
}
