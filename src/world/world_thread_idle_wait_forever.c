#include "fft/world.h"
#include "psx/types.h"

/* Idle thread body: wait one frame, forever. */
void world_thread_idle_wait_forever(void) {
    for (;;) {
        world_thread_wait_frames(1);
    }
}
