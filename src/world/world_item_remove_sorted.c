#include "fft/world.h"

/* Remove an item from a sorted inventory list.
 *
 * Shift the following entries, including the 0xff terminator, into its slot.
 */
void world_item_remove_sorted(s32 item_id, s32 list_index) {
    u8* list;
    s32 index;
    s32 terminator;

    list = g_world_item_sorted_lists[list_index];
    index = 0;
    while (list[index] != item_id) {
        if (list[index] == ITEM_ID_NONE) {
            return;
        }
        index++;
    }
    terminator = ITEM_ID_NONE;
    do {
        list[index] = list[index + 1];
        index++;
    } while (list[index] != terminator);
}
