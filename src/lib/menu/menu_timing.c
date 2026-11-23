/* Menu frame timing and its link to the resident event-script speed. */

#include "fft/menu/menu.h"
#include "fft/script/script.h"
#include "psx/types.h"

#if defined(FFT_WORLD) || defined(FFT_EVENT_CARD) || defined(FFT_EVENT_BUNIT)
/* These programs expose the setter beside the timing query. */
/* Store the raw menu/VSync mode; zero uses script event speed 1. */
void menu_set_event_speed(s32 speed) {
    g_menu_event_speed = speed;
    if (speed == 0) {
        speed = 1;
    }
    script_set_event_speed(speed);
}
#endif

/* Return the menu's frame step (the VSync count per update), treating 0 as 1. */
s32 menu_get_event_speed(void) {
    s32 speed = g_menu_event_speed;

    if (speed == 0) {
        return 1;
    }
    return speed;
}
