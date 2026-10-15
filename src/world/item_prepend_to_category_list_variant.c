#include "fft/world.h"

/* Insert an item at the front of a sorted inventory list.
 *
 * Move the existing entries and their 0xff terminator one slot to the right.
 */
void item_prepend_to_category_list_variant(s32 item_id, s32 list_index) {
    u8* list;
    s32 count;

    list = g_item_category_lists[list_index];
    count = 0;
    while (list[count] != ITEM_ID_NONE) {
        count++;
    }
    for (; count >= 0; count--) {
        list[count + 1] = list[count];
    }
    list[0] = item_id;
}
