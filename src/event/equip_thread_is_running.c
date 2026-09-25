#include "fft/event_equip.h"
#include "psx/types.h"

s32 equip_thread_is_running(s32 thread_id) {
    return g_battle_threads[thread_id].is_running;
}
