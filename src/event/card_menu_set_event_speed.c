#include "fft/event_card.h"
#include "psx/types.h"

void card_menu_set_event_speed(s32 speed) {
    g_card_menu_event_speed = speed;
    if (speed == 0) {
        speed = 1;
    }
    battle_script_set_event_speed(speed);
}
