#include "fft/battle_menu.h"
#include "fft/event_option.h"
#include "fft/menu.h"

/* Thread entry of menu entry 0x1d: load OPTION.OUT and open its AT list. */
void menu_open_option_at_list(void) {
    menu_set_modal_open();
    menu_request_open_companion_executable(COMPANION_OVERLAY_OPTION);
    option_entrypoint(OPTION_MENU_AT_LIST);
}
