#include "fft/thread.h"
#include "psx/types.h"

void battle_thread_suspend(s32 thread_id) {
    g_battle_threads[thread_id].is_running = 0;
}
