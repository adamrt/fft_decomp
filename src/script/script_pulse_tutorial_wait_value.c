#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/* While a tutorial scripts the input, hold wait_value in the tutorial wait
 * variable for one thread yield with every controller input disabled. */
void script_pulse_tutorial_wait_value(s32 wait_value) {
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        script_set_variable(EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE, wait_value);
        script_set_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS, 0xFFFF);
        thread_yield();
        script_set_variable(EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE, 0);
    }
}
