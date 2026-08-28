#include "psx/types.h"

void world_map_data_load_complete_thread(void) {
    world_thread_yield();
    battle_map_mark_data_load_complete();
    world_thread_yield();
    world_thread_exit_current();
}
