#include "fft/event_card.h"
#include "fft/menu.h"

/* Request stop, then clear this screen's managed-thread state. */
void menu_stop_managed_thread(s32 thread_id) {
    menu_request_thread_stop(thread_id);
    g_menu_managed_thread_state = 0;
}
