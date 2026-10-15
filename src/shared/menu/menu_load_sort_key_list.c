#include "fft/event_equip.h"

/* Expand an item-type ordering table into the overlay's working list.
 *
 * The source uses signed bytes terminated by -1, while the destination uses
 * sign-extended s16 entries. */
void menu_load_sort_key_list(s32 list_index, s16* destination) {
    s8* source;
    s32 i;

    source = g_item_type_order_lists[list_index];
    i = 0;
    *destination = source[i];
    while (*destination != -1) {
        i++;
        destination++;
        *destination = source[i];
    }
}
