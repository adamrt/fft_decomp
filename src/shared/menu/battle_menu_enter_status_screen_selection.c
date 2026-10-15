#include "fft/battle_menu.h"

/* Count one more status screen open over the map. Unit animation, the camera
 * animations and the cursor bob freeze while the count is exactly 1. */
void battle_menu_enter_status_screen_selection(void) {
    if (g_battle_menu_status_screen_selected != 0) {
        g_battle_menu_status_screen_selected++;
    } else {
        g_battle_menu_status_screen_selected = 1;
    }
}
