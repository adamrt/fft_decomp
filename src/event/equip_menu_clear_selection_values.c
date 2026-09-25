#include "fft/event_equip.h"
#include "psx/types.h"

void equip_menu_clear_selection_values(void) {
    s32 i;

    for (i = 3; i >= 0; i--) {
        g_equip_menu_selection_values[i] = 0;
    }
}
