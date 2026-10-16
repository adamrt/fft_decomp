/* Setting the menu's frame step and the script event speed. */

#include "fft/menu.h"
#include "fft/script.h"
#include "psx/types.h"

/* Store the raw menu/VSync mode; zero uses script event speed 1. */
void menu_set_vsync_mode_and_event_speed(s32 speed) {
    g_menu_event_speed = speed;
    if (speed == 0) {
        speed = 1;
    }
    script_set_event_speed(speed);
}
