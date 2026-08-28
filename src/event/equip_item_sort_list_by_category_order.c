#include "fft/equip.h"

/* Reorder the selected items according to their category's display order. */
s32 equip_item_sort_list_by_category_order(item_menu_category_e category, s16* list) {
    s16 sorted_items[144];
    u8* category_order;
    s32 category_item_count;
    s32 sorted_count;
    s32 category_order_index;

    category_order = g_equip_item_category_lists[category];
    for (category_item_count = 0; category_order[category_item_count] != EQUIP_ITEM_LIST_END; category_item_count++) { }

    category_order_index = 0;
    sorted_count = 0;
    if (category_item_count > 0) {
        do {
            s32 j;

            for (j = 0; list[j] != -1; j++) {
                if (category_order[category_order_index] == (u8)list[j]) {
                    sorted_items[sorted_count] = list[j];
                    sorted_count++;
                    break;
                }
            }
            category_order_index++;
        } while (category_order_index < category_item_count);
    }

    sorted_items[sorted_count] = -1;
    bcopy(sorted_items, list, (sorted_count + 1) * sizeof(sorted_items[0]));
    return sorted_count;
}
