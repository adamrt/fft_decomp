#include "fft/main.h"
#include "fft/script.h"

/* Return the current event's 16-bit finish-operation value, or 0 while no scenario event
 * is active. Bits 0-7 and 10-11 hold the next scenario id and the remaining bits
 * the after-event code. */
s32 script_get_current_scenario_finish_operation(void) {
    if (g_script_scenario_event_active == 0) {
        return 0;
    }
    return g_main_scenario_event_finish_operations[script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT)];
}
