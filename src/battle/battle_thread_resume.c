#include "fft/battle.h"
#include "psx/types.h"

void battle_thread_resume(s32 thread_id) {
    g_battle_threads[thread_id].is_running = 1;
}
