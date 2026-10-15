#include "fft/event_equip.h"
#include "psx/types.h"

/* Copy the 0x7a-byte record of unit 0 to unit 1, the comparison slot.
 * Neither argument is read. */
void equip_unit_copy_data_to_compare_slot(s32 unused_selected_index, s32 unused_1) {
    bcopy(g_equip_unit_data[0], g_equip_unit_data[1], 0x7a);
}
