#include "fft/event_card.h"
#include "psx/types.h"

s32 card_menu_get_event_speed(void) {
    s32 speed;

    speed = g_card_menu_event_speed;
    if (speed == 0) {
        return 1;
    }
    return speed;
}
