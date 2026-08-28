#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

extern s16 g_world_menu_pending_selection[];

/* On the cancel button, flag the context's thread group as finished,
 * demote every waiting thread task (1 -> 3) and report -1 to the caller. */
void world_menu_cancel_thread_group(world_menu_cancel_context_t* context) {
    s32 i;
    s32 j;
    s32 thread_id;
    s32 count;

    if (!(g_world_menu_new_button_input & PSX_PAD_CROSS)) {
        return;
    }
    if (world_menu_is_input_allowed() == 0) {
        return;
    }
    count = context->thread_count;
    if (count != -1) {
        for (i = 0; i < count; i++) {
            thread_id = i + g_world_thread_current_id;
            g_world_thread_contexts[thread_id].function_parameter_3 = 1;
            if (thread_id == 8) {
                for (j = 3; j >= 0; j--) {
                    g_world_menu_pending_selection[j] = -1;
                }
            }
        }
        for (i = 0; i < 17; i++) {
            if (g_world_thread_contexts[i].task_id == NATIVE_THREAD_TASK_RESUME) {
                g_world_thread_contexts[i].task_id = NATIVE_THREAD_TASK_STOP_REQUEST;
            }
        }
    }
    g_world_menu_new_button_input = 0;
    *context->result_pointer = -1;
    world_sound_set_effect_to_cancel();
    g_world_thread_change_cooldown = 5;
}
