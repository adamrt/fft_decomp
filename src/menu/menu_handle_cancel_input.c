#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/* On the cancel button, flag the context's thread group as finished,
 * demote every waiting thread task (1 -> 3) and report -1 to the caller. */
void menu_handle_cancel_input(menu_entry_t* context) {
    s32 i;
    s32 j;
    s32 thread_id;
    s32 count;

    if (!(g_script_event_input & PSX_PAD_CROSS)) {
        return;
    }
    if (menu_can_accept_input_variant() == 0) {
        return;
    }
    count = context->thread_count;
    if (count != -1) {
        for (i = 0; i < count; i++) {
            thread_id = i + g_thread_current_id;
            g_thread_contexts[thread_id].function_parameter_3 = 1;
            if (thread_id == 8) {
                for (j = 3; j >= 0; j--) {
                    g_menu_pending_selection[j] = -1;
                }
            }
        }
        for (i = 0; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
            if (g_thread_contexts[i].task_id == THREAD_TASK_RESUME) {
                g_thread_contexts[i].task_id = THREAD_TASK_STOP_REQUEST;
            }
        }
    }
    g_script_event_input = 0;
    *menu_entry_result(context) = -1;
    sound_queue_sfx_cancel();
    g_script_input_suppression_frames = 5;
}
