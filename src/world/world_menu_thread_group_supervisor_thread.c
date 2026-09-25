#include "fft/world.h"
#include "psx/types.h"

/* Provisional: 0x24-byte menu thread-group record at 0x80189ecc, indexed by
 * the request word (0-3). Each group names up to three menu threads. */
typedef struct world_menu_thread_group {
    void (*function[3])(void); /* 0x00; -1 when unused */
    s32 thread_id[3];          /* 0x0c; -1 when unused */
    s32 task_id[3];            /* 0x18 */
} world_menu_thread_group_t;

extern world_menu_thread_group_t g_world_menu_thread_groups[];

/* Menu thread-group dispatcher thread. Each request in parameter 1 selects a
 * group: running members with the expected task are signalled to continue,
 * others are signalled to finish (state 1) and awaited, then every finished
 * or idle member is restarted with its function, the matching menu entry, and
 * its task id. The thread exits once no further request is pending. */
void world_menu_thread_group_supervisor_thread(void) {
    s32 state[16];
    world_menu_thread_group_t* group;
    s32 index;
    s32 i;
    s32 thread_id;

    for (;;) {
        index = g_world_threads[g_world_thread_current_id].function_parameter_1;
        g_world_threads[g_world_thread_current_id].function_parameter_1 = 0;
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
                if (g_world_threads[thread_id].is_running != 0) {
                    if (g_world_threads[thread_id].task_id != group->task_id[i]) {
                        state[thread_id] = 1;
                        world_thread_set_parameters(group->thread_id[i], 0, 0, 1);
                    } else {
                        world_thread_set_parameters(thread_id, 0, 1, 0);
                    }
                } else {
                    state[thread_id] = 2;
                }
            }
        }
        for (;;) {
            for (i = 0; i < 3; i++) {
                if (state[group->thread_id[i]] == 1 && g_world_threads[group->thread_id[i]].is_running != 0) {
                    break;
                }
            }
            if (i == 3) {
                break;
            }
            world_thread_wait_frames(1);
        }
        for (i = 0; i < 3; i++) {
            if (state[group->thread_id[i]] != 0 && group->function[i] != (void (*)(void))-1) {
                world_thread_start(group->thread_id[i], group->function[i]);
                world_thread_set_parameters(group->thread_id[i], (s32)&g_world_menu_thread_menu_data[i], 0, 0);
                g_world_threads[group->thread_id[i]].task_id = group->task_id[i];
            }
        }
        if (g_world_threads[g_world_thread_current_id].function_parameter_1 == 0) {
            world_thread_exit_current();
        }
    }
}
