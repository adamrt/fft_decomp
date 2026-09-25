#include "fft/battle.h"
#include "psx/types.h"

void battle_thread_exit_current(void) {
    g_battle_threads[g_battle_current_thread_id].is_running = 0;
    g_battle_threads[g_battle_current_thread_id].task_id = 0;
    battle_thread_yield();
}
