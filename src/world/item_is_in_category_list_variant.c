#include "fft/world.h"

/* Test whether a sorted inventory list contains an item.
 *
 * Stop at the 0xff terminator; entries before it are item IDs.
 */
s32 item_is_in_category_list_variant(s32 item_id, s32 list_index) {
    u8* list;

    list = g_item_category_lists[list_index];
    while (*list != ITEM_ID_NONE) {
        if (*list == item_id) {
            return 1;
        }
        list++;
    }
    return 0;
}
