#include "fft/battle_menu.h"
#include "fft/event_option.h"
#include "fft/menu.h"

/* Thread started for system function 0x1f (crystal or treasure): load OPTION.OUT
 * and open its dead-unit panel. */
void menu_open_option_dead_unit_panel(void) {
    menu_set_modal_open();
    menu_request_open_companion_executable(COMPANION_OVERLAY_OPTION);
    option_entrypoint(OPTION_MENU_DEAD_UNIT);
}
