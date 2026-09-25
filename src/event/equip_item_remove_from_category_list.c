#include "fft/event_equip.h"

/* Remove the first matching item from a category list. */
void equip_item_remove_from_category_list(s32 item_id, s32 category_index) {
    u8* list = g_equip_item_category_lists[category_index];
    s32 i;

    for (i = 0; list[i] != item_id; i++) {
        if (list[i] == EQUIP_ITEM_LIST_END) {
            return;
        }
    }

    do {
        list[i] = list[i + 1];
        i++;
    } while (list[i] != EQUIP_ITEM_LIST_END);
}
