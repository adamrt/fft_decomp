#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Menu thread-group dispatcher thread. Each request in parameter 1 selects a
 * group: running members with the expected task are signaled to continue,
 * others are signaled to finish (state 1) and awaited, then every finished
 * or idle member is restarted with its function, the matching menu entry, and
 * its task id. The thread exits once no further request is pending. */
void world_menu_thread_group_supervisor_thread(void) {
    s32 state[16];
    world_menu_thread_group_t* group;
    s32 index;
    s32 i;
    s32 thread_id;

    for (;;) {
        index = g_threads[g_thread_current_id].function_parameter_1;
        g_threads[g_thread_current_id].function_parameter_1 = 0;
        if (index >= 4) {
            index = 0;
        }
        group = &g_world_menu_thread_groups[index];
        for (i = 15; i >= 0; i--) {
            state[i] = 0;
        }
        for (i = 0; i < 3; i++) {
            thread_id = group->thread_id[i];
            if (thread_id != -1) {
                if (g_threads[thread_id].is_running != 0) {
                    if (g_threads[thread_id].task_id != group->task_id[i]) {
                        state[thread_id] = 1;
                        thread_set_parameters(group->thread_id[i], 0, 0, 1);
                    } else {
                        thread_set_parameters(thread_id, 0, 1, 0);
                    }
                } else {
                    state[thread_id] = 2;
                }
            }
        }
        for (;;) {
            for (i = 0; i < 3; i++) {
                if (state[group->thread_id[i]] == 1 && g_threads[group->thread_id[i]].is_running != 0) {
                    break;
                }
            }
            if (i == 3) {
                break;
            }
            thread_wait_frames(1);
        }
        for (i = 0; i < 3; i++) {
            if (state[group->thread_id[i]] != 0 && group->function[i] != (void (*)(void))-1) {
                thread_start(group->thread_id[i], group->function[i]);
                thread_set_parameters(group->thread_id[i], (s32)&g_menu_thread_menu_data[i], 0, 0);
                g_threads[group->thread_id[i]].task_id = group->task_id[i];
            }
        }
        if (g_threads[g_thread_current_id].function_parameter_1 == 0) {
            thread_exit_current();
        }
    }
}
