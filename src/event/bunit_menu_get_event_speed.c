#include "fft/event_bunit.h"
#include "psx/types.h"

s32 bunit_menu_get_event_speed(void) {
    s32 speed;

    speed = g_bunit_menu_event_speed;
    if (speed == 0) {
        return 1;
    }
    return speed;
}
