#include "fft/event_equip.h"
#include "fft/menu.h"
#include "psx/types.h"

/* The same routine as equip_menu_set_selection_value, compiled twice. equip_menu_update_equipment
 * calls this copy as the menu opens and keeps the result as the selected slot. */
s32 menu_init_selection_value(u8 index, s16 value) {
    g_menu_selection_values[index] = value;
    return value;
}
