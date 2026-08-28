#include "psx/types.h"

void world_thread_wait_frames(s32 ticks) {
    s32 elapsed = 0;

    while (elapsed < ticks) {
        world_thread_yield();
        elapsed++;
    }
}
