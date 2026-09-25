#include "fft/event_equip.h"
#include "psx/types.h"

void equip_menu_clear_selection_records(void) {
    memset(g_equip_menu_selection_records, 0, 0x1E);
}
