#include "fft/item.h"
#include "psx/types.h"

/* Copy one of the item type order lists into destination as halfwords, including its -1
 * terminator. */
void menu_load_sort_key_list(s32 list_index, s16* destination) {
    s8* source = g_item_type_order_lists[list_index];
    s32 i = 0;

    *destination = source[i];
    while (*destination != -1) {
        i++;
        destination++;
        *destination = source[i];
    }
}
