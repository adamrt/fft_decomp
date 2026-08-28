#include "fft/thread.h"

s32 world_thread_resolve_id(s32 thread_id) {
    s32 index;

    if (thread_id < 16) {
        return thread_id;
    }

    index = 1;
    do {
        if (world_thread_is_running_80100164(index) == 0) {
            return index;
        }
        index++;
    } while (index < 17);

    world_thread_exit_current();
}
