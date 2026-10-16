#include "fft/item.h"
#include "psx/types.h"

/* Store the overlay's working item-type order in its packed table.
 *
 * The source uses s16 entries terminated by -1; only each entry's low byte is
 * written. */
void menu_store_sort_key_list(s32 list_index, const s16* source) {
    s8* destination;
    s32 i;

    destination = g_item_type_order_lists[list_index];
    i = 0;
    while ((destination[i] = source[i]) != -1) {
        i++;
    }
}
