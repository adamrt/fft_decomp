#include "fft/event_equip.h"

/* Returns whether an item occurs in a 0xFF-terminated category list. */
s32 equip_item_is_in_category_list(s32 item_id, s32 category_index) {
    u8* item;

    for (item = g_equip_item_category_lists[category_index]; *item != EQUIP_ITEM_LIST_END; item++) {
        if (*item == item_id) {
            return 1;
        }
    }
    return 0;
}
