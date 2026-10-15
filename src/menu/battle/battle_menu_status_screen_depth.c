/* Status-screen open count and the animation speed accessors. */

#include "fft/battle.h"
#include "fft/battle_menu.h"
#include "psx/types.h"

void battle_menu_enter_status_screen_selection(void) {
    if (g_battle_menu_status_screen_selected != 0) {
        g_battle_menu_status_screen_selected++;
    } else {
        g_battle_menu_status_screen_selected = 1;
    }
}

/* Reset the status-screen count to 0, whatever its depth. */
void battle_menu_leave_status_screen_selection(void) {
    g_battle_menu_status_screen_selected = 0;
}

s32 battle_state_get_animation_speed(void) {
    return g_main_system_animation_speed;
}

/* Set g_main_system_animation_speed, the number of vertical blanks per frame, to
 * 1 or 2. Returns 0, or -1 for any other value. */
s32 battle_state_set_animation_speed(s32 speed) {
    if (speed == 1 || speed == 2) {
        g_main_system_animation_speed = speed;
        return 0;
    }
    return -1;
}
