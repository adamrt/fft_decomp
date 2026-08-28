#include "fft/world.h"

/* Test whether a sorted inventory list contains an item.
 *
 * Stop at the 0xff terminator; entries before it are item IDs.
 */
s32 world_item_is_in_sorted_list(s32 item_id, s32 list_index) {
    u8* list;

    list = g_world_item_sorted_lists[list_index];
    while (*list != ITEM_ID_NONE) {
        if (*list == item_id) {
            return 1;
        }
        list++;
    }
    return 0;
}
