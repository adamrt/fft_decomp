#include "fft/battle.h"
#include "psx/types.h"

/* Read pad 1 for the deployment screen and derive the pressed and released
 * buttons; the reset button combination resets the game. Always returns 1, so the
 * -1 that battle_state_run_deployment tests for does not occur. */
s32 battle_state_update_deployment_controller_input(void) {
    u32 previous;
    u32 current;

    current = PadRead(1);
    previous = g_input_previous_buttons;
    g_input_current_buttons = current;
    g_input_previous_buttons = current;
    g_main_input_pressed_buttons = ~previous & current;
    g_main_input_released_buttons = ~current & previous;

    if ((current & MAIN_INPUT_RESET_BUTTONS) == MAIN_INPUT_RESET_BUTTONS) {
        main_system_reset_game();
    }
    return 1;
}
