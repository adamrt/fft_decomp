#include "fft/battle_menu.h"
#include "fft/battle_status.h"

void battle_menu_clear_status_menu_state(void) {
    g_battle_menu_status_requested = 0;
    g_status_menu_open = 0;
}
