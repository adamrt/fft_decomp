#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

void menu_finish_at_list_selection(void) {
    g_menu_at_list_selection_finished = 1;
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_finish_at_list_selection and menu_noop_6. Nothing on the disc calls it
 * or points at it; the name is a neutral placeholder until a reference turns
 * up. */
void menu_noop_5(void) {
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between menu_noop_5 and
 * menu_start_unit_view_thread. Nothing on the disc calls it or points at it;
 * the name is a neutral placeholder until a reference turns up. */
void menu_noop_6(void) {
}

/* Requests a unit view mode from the unit view supervisor thread, starting the
 * thread when it is not running.
 *
 * The unit name list is rebuilt and the panel fade mode taken from
 * g_menu_panel_fade_modes[mode]. unit_a becomes the active-turn unit of
 * the banner and unit_b the previewed target of the status billboard; 0xff leaves
 * either unchanged. Ignored while g_menu_overlay_state is 1. */
void menu_start_unit_view_thread(s32 mode, s32 unit_a, s32 unit_b) {
    if (g_menu_overlay_state == 1) {
        return;
    }
    text_build_unit_name_list();
    g_menu_panel_fade_mode = g_menu_panel_fade_modes[mode];
    if (unit_a != 0xff) {
        g_menu_selected_unit.battle_id = unit_a;
        g_menu_selected_unit_status.unit_index = unit_a;
    }
    if (unit_b != 0xff) {
        g_menu_comparison_unit_data.battle_id = unit_b;
        g_menu_comparison_unit_status.unit_index = unit_b;
    }
    if (thread_is_running_asm_variant(THREAD_SLOT_UNIT_VIEW) == 0) {
        thread_start(THREAD_SLOT_UNIT_VIEW, menu_run_unit_view_supervisor_thread);
    }
    thread_set_parameters(THREAD_SLOT_UNIT_VIEW, mode, 0, 0);
}

enum {
    MENU_UNIT_VIEW_SLOT_IDLE = 0,
    MENU_UNIT_VIEW_SLOT_RESTART = 1,
    MENU_UNIT_VIEW_SLOT_STARTED = 2,
};

/* Starts the requested unit-view panel threads, stops stale panels, and restarts
 * them after they exit. A new parameter replaces the pending view request. */
void menu_run_unit_view_supervisor_thread(void) {
    s32 states[16];
    menu_unit_view_thread_set_t* set;
    s32 mode;
    s32 i;
    s32 task;
    thread_t* thread;

    for (;;) {
        mode = thread_get_current_parameter_1_variant();
        set = &g_menu_unit_view_thread_sets[mode];
        g_menu_unit_view_mode = mode;
        g_thread_contexts[g_thread_current_id].function_parameter_1 = 0xff;
        for (i = 15; i >= 0; i--) {
            states[i] = MENU_UNIT_VIEW_SLOT_IDLE;
        }
        for (i = 0; i < 4; i++) {
            thread = &g_thread_contexts[set->thread_ids[i]];
            if (thread->is_running != 0) {
                task = thread->task_id;
                if (task == set->task_ids[i]
                    && (task != THREAD_TASK_HOVERED_UNIT_STATS
                        || g_menu_unit_view_cached_battle_id == g_menu_selected_unit.battle_id)
                    && (task != THREAD_TASK_RIGHT_UNIT_STATS
                        || g_menu_unit_view_cached_comparison_id == g_menu_comparison_unit_data.battle_id)
                    && (task != THREAD_TASK_UNIT_SUMMARY_PANEL
                        || g_menu_unit_view_cached_battle_id == g_menu_selected_unit.battle_id)) {
                    continue;
                }
                states[set->thread_ids[i]] = MENU_UNIT_VIEW_SLOT_RESTART;
                thread_set_parameters(set->thread_ids[i], 0, 0, 1);
            } else if (set->entries[i] != (void*)-1) {
                thread_start(set->thread_ids[i], set->entries[i]);
                thread_set_parameters(set->thread_ids[i], 0, 0, 0);
                thread->task_id = set->task_ids[i];
                states[set->thread_ids[i]] = MENU_UNIT_VIEW_SLOT_STARTED;
            }
        }
        g_menu_unit_view_cached_battle_id = g_menu_selected_unit.battle_id;
        g_menu_unit_view_cached_comparison_id = g_menu_comparison_unit_data.battle_id;
        for (;;) {
            for (i = 0; i < 4; i++) {
                if (states[set->thread_ids[i]] == MENU_UNIT_VIEW_SLOT_RESTART
                    && g_thread_contexts[set->thread_ids[i]].is_running != 0) {
                    break;
                }
            }
            if (i == 4) {
                break;
            }
            thread_wait_frames(1);
        }
        for (i = 0; i < 4; i++) {
            if (states[set->thread_ids[i]] == MENU_UNIT_VIEW_SLOT_RESTART && set->entries[i] != (void*)-1) {
                thread_start(set->thread_ids[i], set->entries[i]);
                thread_set_parameters(set->thread_ids[i], 0, 0, 0);
                g_thread_contexts[set->thread_ids[i]].task_id = set->task_ids[i];
            }
        }
        if (thread_get_current_parameter_1_variant() == 0xff) {
            thread_exit_current();
        }
    }
}
