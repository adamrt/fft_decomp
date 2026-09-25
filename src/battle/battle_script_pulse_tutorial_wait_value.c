#include "fft/battle.h"
#include "psx/types.h"

/* While a tutorial scripts the input, hold wait_value in the tutorial wait
 * variable for one thread yield with every controller input disabled. */
void battle_script_pulse_tutorial_wait_value(s32 wait_value) {
    if (g_battle_menu_input_disabled == 2) {
        battle_script_set_variable(EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE, wait_value);
        battle_script_set_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS, 0xFFFF);
        battle_thread_yield();
        battle_script_set_variable(EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE, 0);
    }
}
