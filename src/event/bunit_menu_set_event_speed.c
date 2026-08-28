#include "fft/bunit.h"
#include "fft/event.h"
#include "psx/types.h"

void bunit_menu_set_event_speed(s32 speed) {
    g_bunit_menu_event_speed = speed;
    if (speed == 0) {
        speed = 1;
    }
    battle_script_set_event_speed(speed);
}
