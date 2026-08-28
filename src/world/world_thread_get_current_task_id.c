#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

s32 world_thread_get_current_task_id(void) {
    return g_world_thread_task_ids[g_world_thread_current_id][0];
}
