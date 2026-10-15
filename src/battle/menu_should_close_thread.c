#include "fft/battle_event.h"
#include "fft/battle_menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Polled by a menu thread each frame; input is the frame's 32-bit input value.
 *
 * While input is locked or a help window is open the input is cleared and 0
 * returned. If another running thread carries the stop-request task id, the input
 * is cleared too. Returns 1 only when no such thread exists and the current
 * thread's third parameter is set, which is how a menu thread is asked to close. */
s32 menu_should_close_thread(u32* input) {
    s32 i;
    s32 task_state;

    if (g_menu_input_lock_mode != 0 || g_menu_help_open != 0) {
        *input = 0;
        return 0;
    }
    i = 0;
    task_state = 3;
    for (; i < 16; i++) {
        if (i != g_thread_current_id && thread_is_running_asm_variant(i) != 0
            && g_thread_contexts[i].task_id == task_state) {
            *input = 0;
            break;
        }
    }
    if (i == 16 && thread_get_current_parameter_3_variant() != 0) {
        return 1;
    }
    return 0;
}
