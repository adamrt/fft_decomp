#include "fft/world.h"

/* Return the active scenario's event-finish operations, or 0 outside a scenario. */
u16 world_script_get_current_scenario_finish_operation(void) {
    u16 result;

    if (g_world_menu_scenario_event_started != 0) {
        result = g_scenario_event_finish_operations[world_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT)];
    } else {
        result = 0;
    }
    return result;
}
