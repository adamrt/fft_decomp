/* The event thread body that loads the selected event and interprets it. */

#include "fft/script.h"

/* Event-thread body started by script_start_current_event_thread or
 * script_start_current_event_thread: load the selected event and interpret it. */
void script_run_next_event(void) {
    script_load_next_event();
    script_execute_event_variant();
}
