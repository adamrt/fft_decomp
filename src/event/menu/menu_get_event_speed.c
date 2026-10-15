#include "fft/menu.h"

s32 menu_get_event_speed(void) {
    s32 speed;

    speed = g_menu_event_speed;
    if (speed == 0) {
        return 1;
    }
    return speed;
}
