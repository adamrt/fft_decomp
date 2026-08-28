#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_enter_status_screen_selection(void) {
    if (g_battle_menu_status_screen_selected != 0) {
        g_battle_menu_status_screen_selected++;
    } else {
        g_battle_menu_status_screen_selected = 1;
    }
}
