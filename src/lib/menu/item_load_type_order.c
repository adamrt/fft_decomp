/* Loading a packed sort-key order as halfwords. */

#include "fft/item.h"
#include "psx/types.h"

/* Copy a sort-key order into destination as halfwords, including its -1
 * terminator. WORLD slot 0 holds Formation keys; slots 1..5 hold item keys. */
void item_load_type_order(s32 list_index, s16* destination) {
    s8* source = g_item_type_order_lists[list_index];
    s32 i = 0;

    *destination = source[i];
    while (*destination != -1) {
        i++;
        destination++;
        *destination = source[i];
    }
}
