/* Putting an item first in a category list. */

#include "fft/item.h"
#include "psx/types.h"

/* Prepend an item to the selected category list.
 *
 * The existing entries and their terminator move one byte to the right. */
void item_prepend_to_category_list(s32 item_id, s32 category_index) {
    u8* list = g_item_category_lists[category_index];
    s32 i = 0;

    while (list[i] != ITEM_LIST_END) {
        i++;
    }
    for (; i >= 0; i--) {
        list[i + 1] = list[i];
    }
    *list = item_id;
}
