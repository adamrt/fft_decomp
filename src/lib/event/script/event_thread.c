/* The event thread body that loads the selected event and interprets it. */

#include "fft/script/script.h"

/* Event-thread body that script_start_current_event_thread starts in THREAD_SLOT_EVENT: load
 * the event the current-event variable selects and interpret it. */
void script_event_thread(void) {
    script_load_next_event();
    script_execute_event();
}
