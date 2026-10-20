/* Start a menu dialog in an idle slot or poll the currently requested text. */

#include "fft/message.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Start a dialog with the packed dialog type, text ID and speaker selector
 * only when its scheduler slot is idle. */
void menu_text_start_thread_if_idle(s32 thread_id, s32 dialog_type, s32 text_id, s32 speaker_id) {
    if (thread_is_running(thread_id) == 0) {
        thread_start(thread_id, message_dialog_thread);
        thread_parameters_set(thread_id, dialog_type, text_id, speaker_id);
    }
}

#if !defined(FFT_EVENT_EQUIP)
/* CARD and WORLD also track a dialog request until it is acknowledged. */
/* Zero means no pending request; this key is shared across scheduler slots. */
extern s32 g_menu_text_active_key;

/* Track one pending dialog by text ID. An idle requested slot acknowledges
 * a matching ID and returns 0. Otherwise start the dialog and return 1 when
 * no ID is pending, or return the requested slot's running flag. */
s32 menu_text_start_or_poll_thread(s32 thread_id, s32 dialog_type, s32 text_id, s32 speaker_id, s32 y_offset) {
    s32 status = thread_is_running(thread_id);

    if (status == 0) {
        if (g_menu_text_active_key == text_id) {
            g_menu_text_active_key = 0;
            return 0;
        }
    }
    if (g_menu_text_active_key == 0) {
        thread_start(thread_id, message_dialog_thread);
        thread_parameters_set(thread_id, dialog_type, text_id, speaker_id);
        g_menu_text_active_key = text_id;
        g_threads[thread_id].task_words[DIALOG_THREAD_WORD_Y_OFFSET] = y_offset;
        return 1;
    }
    return status;
}
#endif
