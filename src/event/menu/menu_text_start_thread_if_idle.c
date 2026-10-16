/* Starting a text thread only when its scheduler slot is idle. */

#include "fft/text.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Start the selected text thread only when its scheduler slot is idle. */
void menu_text_start_thread_if_idle(s32 thread_id, s32 parameter_0, s32 parameter_1, s32 parameter_2) {
    if (thread_is_running(thread_id) == 0) {
        thread_start(thread_id, message_dialog_thread);
        thread_set_parameters(thread_id, parameter_0, parameter_1, parameter_2);
    }
}
