#include "fft/battle_action.h"
#include "fft/battle_menu.h"

/* Enter the event state and run the scenario check with no unit (0xff), as the
 * deployment screen does when it finishes. */
void action_set_casting_unit_id_ff_and_init(void) {
    action_set_casting_unit_id_ff();
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 0, 0xff, 0, 1);
}
