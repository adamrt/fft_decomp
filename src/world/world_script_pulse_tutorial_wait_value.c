#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/types.h"

void world_script_pulse_tutorial_wait_value(s32 value) {
    if (g_world_menu_input_disabled == 2) {
        world_script_set_variable(EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE, value);
        world_script_set_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS, 0xFFFF);
        world_thread_yield();
        world_script_set_variable(EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE, 0);
    }
}
