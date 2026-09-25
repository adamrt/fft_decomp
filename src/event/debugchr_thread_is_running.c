#include "fft/event_debugchr.h"
#include "psx/types.h"

s32 debugchr_thread_is_running(s32 thread_id) {
    return g_battle_threads[thread_id].is_running;
}
