#include "fft/data.h"
#include "fft/world.h"

void world_thread_set_current_task_id(s32 task_id) {
    g_world_thread_task_ids[g_world_thread_current_id][0] = task_id;
}
