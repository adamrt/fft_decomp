#include "fft/thread.h"
#include "fft/world.h"

enum {
    WORLD_UNIT_VIEW_SLOT_IDLE = 0,
    WORLD_UNIT_VIEW_SLOT_RESTART = 1,
    WORLD_UNIT_VIEW_SLOT_STARTED = 2,
};

/*
 * Unit-view supervisor thread started by world_unit_start_view_thread.
 *
 * For the requested view mode it starts each missing slot thread, and asks
 * running ones with the wrong task (or a stale viewed unit) to stop, then
 * restarts them once they have exited. Parameter 1 is reset to 0xff so a
 * new request can be detected; the thread exits when none arrives.
 */
void world_unit_view_supervisor_thread(void) {
    s32 states[16];
    world_unit_view_thread_set_t* set;
    s32 mode;
    s32 i;
    s32 task;
    native_thread_t* thread;

    for (;;) {
        mode = (s32)world_thread_get_current_parameter_1();
        set = &g_world_unit_view_thread_sets[mode];
        g_world_unit_view_mode = mode;
        g_world_thread_contexts[g_world_thread_current_id].function_parameter_1 = 0xff;
        for (i = 15; i >= 0; i--) {
            states[i] = WORLD_UNIT_VIEW_SLOT_IDLE;
        }
        for (i = 0; i < 4; i++) {
            thread = &g_world_thread_contexts[set->thread_ids[i]];
            if (thread->is_running != 0) {
                task = thread->task_id;
                if (task == set->task_ids[i]
                    && (task != 0x11 || g_world_unit_view_started_battle_id == g_world_unit_view_battle_id)
                    && (task != 0x3a || g_world_unit_view_started_comparison_id == g_world_unit_comparison_battle_id)
                    && (task != 0x14 || g_world_unit_view_started_battle_id == g_world_unit_view_battle_id)) {
                    continue;
                }
                states[set->thread_ids[i]] = WORLD_UNIT_VIEW_SLOT_RESTART;
                world_thread_set_parameters(set->thread_ids[i], 0, 0, 1);
            } else if (set->entries[i] != (void*)-1) {
                world_thread_start(set->thread_ids[i], set->entries[i]);
                world_thread_set_parameters(set->thread_ids[i], 0, 0, 0);
                thread->task_id = set->task_ids[i];
                states[set->thread_ids[i]] = WORLD_UNIT_VIEW_SLOT_STARTED;
            }
        }
        g_world_unit_view_started_battle_id = g_world_unit_view_battle_id;
        g_world_unit_view_started_comparison_id = g_world_unit_comparison_battle_id;
        for (;;) {
            for (i = 0; i < 4; i++) {
                if (states[set->thread_ids[i]] == WORLD_UNIT_VIEW_SLOT_RESTART
                    && g_world_thread_contexts[set->thread_ids[i]].is_running != 0) {
                    break;
                }
            }
            if (i == 4) {
                break;
            }
            world_thread_wait_frames(1);
        }
        for (i = 0; i < 4; i++) {
            if (states[set->thread_ids[i]] == WORLD_UNIT_VIEW_SLOT_RESTART && set->entries[i] != (void*)-1) {
                world_thread_start(set->thread_ids[i], set->entries[i]);
                world_thread_set_parameters(set->thread_ids[i], 0, 0, 0);
                g_world_thread_contexts[set->thread_ids[i]].task_id = set->task_ids[i];
            }
        }
        if ((s32)world_thread_get_current_parameter_1() == 0xff) {
            world_thread_exit_current();
        }
    }
}
