#include "fft/thread.h"

s32 helpmenu_thread_find_running_from_4(void) {
    s32 thread_id;
    native_thread_t* thread;

    thread_id = 4;
    thread = g_battle_threads + 4;
    do {
        if (thread->is_running == 1) {
            return thread_id;
        }
        thread_id++;
        thread++;
    } while (thread_id < 9);
    return thread_id;
}
