#include "fft/script.h"
#include "psx/types.h"

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * script_start_current_event_thread and script_reset_event_state. Nothing on
 * the disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void script_noop_8(void) {
}
