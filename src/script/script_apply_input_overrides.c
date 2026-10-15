#include "fft/main.h"
#include "fft/script.h"

/* Apply disabled and forced input variables while retaining the high input bits. */
void script_apply_input_overrides(u32* state) {
    u32 high;

    if (*state == MAIN_INPUT_RESET_BUTTONS) {
        main_system_reset_game();
    }

    high = *state & 0xFFFF0000;
    *state = *state & (script_get_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS) ^ 0xFFFF);
    *state = high | (script_get_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS) | *state);
}
