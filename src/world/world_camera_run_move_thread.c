#include "psx/types.h"

void world_camera_run_move_thread(void) {
    do {
        world_thread_yield();
    } while (battle_camera_is_active() != 0);
}
