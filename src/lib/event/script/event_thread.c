/* The event thread body that loads the selected event and interprets it. */

#include "fft/script.h"

/* Event-thread body that script_current_event_thread_start starts in THREAD_SLOT_EVENT: load
 * the event the current-event variable selects and interpret it. */
void script_event_thread_run(void) {
    script_next_event_load();
    script_execute_event();
}
