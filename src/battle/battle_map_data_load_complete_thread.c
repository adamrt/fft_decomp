#include "fft/battle.h"
#include "psx/types.h"

void battle_map_data_load_complete_thread(void) {
    battle_thread_yield();
    battle_map_mark_data_load_complete();
    battle_thread_yield();
    battle_thread_exit_current();
}
