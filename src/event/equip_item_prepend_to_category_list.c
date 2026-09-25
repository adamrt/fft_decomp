#include "fft/event_equip.h"

/* Prepend an item to the selected category list.
 *
 * The existing entries and their terminator move one byte to the right. */
void equip_item_prepend_to_category_list(s32 item_id, s32 category_index) {
    u8* list;
    s32 i;

    list = g_equip_item_category_lists[category_index];
    i = 0;
    while (list[i] != EQUIP_ITEM_LIST_END) {
        i++;
    }
    for (; i >= 0; i--) {
        list[i + 1] = list[i];
    }
    *list = item_id;
}
