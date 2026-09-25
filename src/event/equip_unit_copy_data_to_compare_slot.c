#include "fft/event_equip.h"
#include "psx/types.h"

void equip_unit_copy_data_to_compare_slot(void) {
    bcopy((u8*)g_equip_unit_data[0], (u8*)g_equip_unit_data[1], 0x7a);
}
