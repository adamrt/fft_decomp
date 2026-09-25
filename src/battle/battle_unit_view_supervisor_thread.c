#include "fft/battle.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Provisional: 0x30-byte unit-view thread set at 0x80169040, indexed by
 * the view mode. Each of the four slots names a thread entry (or -1), the
 * scheduler slot it runs in, and the task id it must carry. */
typedef world_unit_view_thread_set_t battle_unit_view_thread_set_t;
typedef char battle_unit_view_thread_set_size_must_be_0x30[(sizeof(battle_unit_view_thread_set_t) == 0x30) ? 1 : -1];

enum {
    BATTLE_UNIT_VIEW_SLOT_IDLE = 0,
    BATTLE_UNIT_VIEW_SLOT_RESTART = 1,
    BATTLE_UNIT_VIEW_SLOT_STARTED = 2,
};

extern battle_unit_view_thread_set_t g_battle_unit_view_thread_sets[];

/*
 * Battle twin of world_unit_view_supervisor_thread; started by
 * battle_menu_store_unit_names_and_event_block_data.
 *
 * For the requested view mode it starts each missing slot thread, and asks
 * running ones with the wrong task (or a stale viewed unit) to stop, then
 * restarts them once they have exited. Parameter 1 is reset to 0xff so a
 * new request can be detected; the thread exits when none arrives.
 */
void battle_unit_view_supervisor_thread(void) {
    s32 states[16];
    battle_unit_view_thread_set_t* set;
    s32 mode;
    s32 i;
    s32 task;
    native_thread_t* thread;

    for (;;) {
        mode = (s32)battle_thread_get_current_parameter_1();
        set = &g_battle_unit_view_thread_sets[mode];
        g_battle_unit_view_mode = mode;
        g_battle_thread_contexts[g_battle_current_thread_id].function_parameter_1 = 0xff;
        for (i = 15; i >= 0; i--) {
            states[i] = BATTLE_UNIT_VIEW_SLOT_IDLE;
        }
        for (i = 0; i < 4; i++) {
            thread = &g_battle_thread_contexts[set->thread_ids[i]];
            if (thread->is_running != 0) {
                task = thread->task_id;
                if (task == set->task_ids[i]
                    && (task != 0x11 || g_battle_unit_view_started_battle_id == g_battle_active_turn_unit.battle_id)
                    && (task != 0x3a || g_battle_unit_view_started_comparison_id == g_battle_preview_target_unit_id)
                    && (task != 0x14 || g_battle_unit_view_started_battle_id == g_battle_active_turn_unit.battle_id)) {
                    continue;
                }
                states[set->thread_ids[i]] = BATTLE_UNIT_VIEW_SLOT_RESTART;
                battle_thread_set_parameters(set->thread_ids[i], 0, 0, 1);
            } else if (set->entries[i] != (void*)-1) {
                battle_thread_start(set->thread_ids[i], set->entries[i]);
                battle_thread_set_parameters(set->thread_ids[i], 0, 0, 0);
                thread->task_id = set->task_ids[i];
                states[set->thread_ids[i]] = BATTLE_UNIT_VIEW_SLOT_STARTED;
            }
        }
        g_battle_unit_view_started_battle_id = g_battle_active_turn_unit.battle_id;
        g_battle_unit_view_started_comparison_id = g_battle_preview_target_unit_id;
        for (;;) {
            for (i = 0; i < 4; i++) {
                if (states[set->thread_ids[i]] == BATTLE_UNIT_VIEW_SLOT_RESTART
                    && g_battle_thread_contexts[set->thread_ids[i]].is_running != 0) {
                    break;
                }
            }
            if (i == 4) {
                break;
            }
            battle_thread_wait_frames(1);
        }
        for (i = 0; i < 4; i++) {
            if (states[set->thread_ids[i]] == BATTLE_UNIT_VIEW_SLOT_RESTART && set->entries[i] != (void*)-1) {
                battle_thread_start(set->thread_ids[i], set->entries[i]);
                battle_thread_set_parameters(set->thread_ids[i], 0, 0, 0);
                g_battle_thread_contexts[set->thread_ids[i]].task_id = set->task_ids[i];
            }
        }
        if ((s32)battle_thread_get_current_parameter_1() == 0xff) {
            battle_thread_exit_current();
        }
    }
}
