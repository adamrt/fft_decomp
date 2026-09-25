#include "fft/world.h"
#include "psx/types.h"

/* Refresh the low halfword of a menu state word from the disabled and forced
 * controller-input variables, preserving its high halfword. The sentinel
 * 0x90C requests a full game reset before the refresh. */
void world_menu_refresh_state_from_script_variables(s32* state) {
    s32 high;

    if (*state == 0x90C) {
        main_system_reset_game();
    }

    high = *state & 0xFFFF0000;
    *state = *state & (world_script_get_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS) ^ 0xFFFF);
    *state = high | (world_script_get_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS) | *state);
}
