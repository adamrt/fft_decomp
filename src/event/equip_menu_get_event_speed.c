#include "fft/equip.h"
#include "psx/types.h"

s32 equip_menu_get_event_speed(void) {
    s32 value = g_equip_menu_event_speed;

    if (value == 0) {
        return 1;
    }
    return value;
}
