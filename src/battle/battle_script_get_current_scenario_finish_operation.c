#include "fft/battle.h"

s32 battle_script_get_current_scenario_finish_operation(void) {
    if (g_battle_scenario_event_active == 0) {
        return 0;
    }
    return g_scenario_event_finish_operations[battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT)];
}
