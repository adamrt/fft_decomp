/* Starting the text thread when no text is pending, or polling the pending text. */

#include "fft/menu_text.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Starts the text character handling thread on thread_id with the given
 * parameters when no text is pending. Returns 0 once the pending text
 * matches `key` and has finished, 1 when a new thread was started, otherwise
 * the running status. */
s32 menu_text_start_or_poll_thread(s32 thread_id, s32 parameter_0, s32 key, s32 parameter_2, s32 y_offset) {
    s32 status = thread_is_running(thread_id);

    if (status == 0) {
        if (g_menu_text_active_key == key) {
            g_menu_text_active_key = 0;
            return 0;
        }
    }
    if (g_menu_text_active_key == 0) {
        thread_start(thread_id, text_run_dialog_thread);
        thread_set_parameters(thread_id, parameter_0, key, parameter_2);
        g_menu_text_active_key = key;
        g_threads[thread_id].task_words[DIALOG_THREAD_WORD_Y_OFFSET] = y_offset;
        return 1;
    }
    return status;
}
