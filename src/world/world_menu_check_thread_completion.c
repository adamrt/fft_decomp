#include "fft/world.h"
#include "psx/pad.h"

/*
 * Suppress menu input while another task owns the UI and check completion.
 *
 * Input bit 0x100 opens the entry's auxiliary text thread when its table
 * selector is nonzero. Completion waits for all other task-3 threads.
 */
s32 world_menu_check_thread_completion(s32* input) {
    world_menu_entry_t* parameter;
    s32 i;

    if (g_world_thread_contexts[1].is_running == 0 && world_menu_is_input_allowed() != 0 && (*input & PSX_PAD_SELECT)) {
        parameter = world_thread_get_current_parameter_1();
        if (parameter->select_text_table != 0) {
            if (g_world_thread_change_cooldown == 0) {
                world_thread_start(1, world_menu_announce_entry_value_thread);
                world_thread_set_parameters(1, (s32)parameter, 0, 0);
                g_world_thread_task_active = 1;
            }
            *input = 0;
        }
    }
    if (g_world_thread_task_active != 0 || g_world_text_overlay_active != 0) {
        *input = 0;
        return 0;
    }
    for (i = 0; i < 17; i++) {
        if (i != g_world_thread_current_id && world_thread_is_running_80100164(i) != 0
            && g_world_thread_contexts[i].task_id == NATIVE_THREAD_TASK_STOP_REQUEST) {
            *input = 0;
            break;
        }
    }
    if (i == 17 && world_thread_get_current_parameter_3() != 0) {
        return 1;
    }
    return 0;
}
