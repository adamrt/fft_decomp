#include "fft/world.h"

/* Yield to the next WORLD thread, forever. */
void world_thread_yield_forever(void) {
    for (;;) {
        world_thread_yield();
    }
}
