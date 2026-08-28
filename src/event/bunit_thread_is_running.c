#include "fft/thread.h"
#include "psx/types.h"

s32 bunit_thread_is_running(s32 thread_id) {
    thread_id = (thread_id * NATIVE_THREAD_STRIDE) + (s32)g_battle_threads;
    return *(s32*)(thread_id + 0x48);
}
