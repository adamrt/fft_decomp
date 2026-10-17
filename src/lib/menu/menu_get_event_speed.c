/* The menu's frame step, treating 0 as 1. */

#include "fft/menu.h"
#include "psx/types.h"

/* Return the menu's frame step (the VSync count per update), treating 0 as 1. */
s32 menu_get_event_speed(void) {
    s32 speed = g_menu_event_speed;

    if (speed == 0) {
        return 1;
    }
    return speed;
}
