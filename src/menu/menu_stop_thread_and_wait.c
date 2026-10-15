#include "fft/event_card.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Ask a thread to stop and run input-update frames until it has ended; then
 * clear the managed-thread state. */
void menu_stop_thread_and_wait(s32 id) {
    s32 output;

    while (thread_is_running(id) != 0) {
        menu_request_thread_stop(id);
        menu_input_update_event_state_variant(&output, 0, 0);
    }
    g_menu_managed_thread_state = 0;
}
