#include "fft/world.h"

/* Idle thread body: yield to the next WORLD thread, forever. */
void world_thread_idle_yield_forever(void) {
    for (;;) {
        world_thread_yield();
    }
}
