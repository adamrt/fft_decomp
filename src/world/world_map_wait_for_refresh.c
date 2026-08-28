#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Yield until g_world_map_refresh_pending is cleared by another thread. */
void world_map_wait_for_refresh(void) {
    if (g_world_map_refresh_pending != 0) {
        do {
            world_thread_yield();
        } while (g_world_map_refresh_pending != 0);
    }
}
