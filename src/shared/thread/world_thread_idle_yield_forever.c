#include "fft/thread.h"
#include "fft/world.h"

/* Idle thread body: yield to the next WORLD thread, forever. */
void world_thread_idle_yield_forever(void) {
    for (;;) {
        thread_yield();
    }
}
