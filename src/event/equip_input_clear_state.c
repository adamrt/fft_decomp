#include "fft/event_equip.h"
#include "psx/types.h"

void equip_input_clear_state(void) {
    g_equip_input_newly_pressed = 0;
    g_equip_input_secondary_repeat = 0;
    g_equip_input_primary_repeat = 0;
}
