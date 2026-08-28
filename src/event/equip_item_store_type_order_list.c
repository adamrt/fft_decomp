#include "fft/equip.h"

/* Store the overlay's working item-type order in its packed table.
 *
 * The source uses s16 entries terminated by -1; only each entry's low byte is
 * written. */
void equip_item_store_type_order_list(s32 list_index, const s16* source) {
    u8* destination;
    s32 i;

    destination = g_equip_item_type_order_lists[list_index];
    i = 0;
    while ((s8)(destination[i] = source[i]) != -1) {
        i++;
    }
}
