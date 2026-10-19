/* Starting a text thread only when its scheduler slot is idle. */

#include "fft/message.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Start a dialog with the packed dialog type, text ID and speaker selector
 * only when its scheduler slot is idle. */
void menu_text_start_thread_if_idle(s32 thread_id, s32 dialog_type, s32 text_id, s32 speaker_id) {
    if (thread_is_running(thread_id) == 0) {
        thread_start(thread_id, message_dialog_thread);
        thread_set_parameters(thread_id, dialog_type, text_id, speaker_id);
    }
}
