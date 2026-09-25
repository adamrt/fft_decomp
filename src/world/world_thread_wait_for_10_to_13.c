#include "fft/world.h"
#include "psx/types.h"

void world_thread_wait_for_10_to_13(void) {
    world_thread_wait_until_inactive(0xD);
    world_thread_wait_until_inactive(0xC);
    world_thread_wait_until_inactive(0xB);
    world_thread_wait_until_inactive(0xA);
}
