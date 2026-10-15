#include "fft/event_equip.h"

/* Expand an item-type ordering table into the overlay's working list.
 *
 * The source uses signed bytes terminated by -1, while the destination uses
 * sign-extended s16 entries. */
void menu_load_sort_key_list(s32 list_index, s16* destination) {
    s8* source;
    s32 i;

    source = g_item_type_order_lists[list_index];
    i = 0;
    *destination = source[i];
    while (*destination != -1) {
        i++;
        destination++;
        *destination = source[i];
    }
}

/* Store the overlay's working item-type order in its packed table.
 *
 * The source uses s16 entries terminated by -1; only each entry's low byte is
 * written. */
void menu_store_sort_key_list(s32 list_index, const s16* source) {
    s8* destination;
    s32 i;

    destination = g_item_type_order_lists[list_index];
    i = 0;
    while ((destination[i] = source[i]) != -1) {
        i++;
    }
}

/* Compact a -1-terminated s16 item list and append its byte terminator. */
void item_store_category_list(s32 category_index, const s16* source) {
    u8* base;
    s32 count;

    base = g_item_category_lists[category_index];
    count = 0;
    while (*source != -1) {
        base[count] = *source++;
        count++;
    }
    base[count] = EQUIP_ITEM_LIST_END;
}

/* Reorder the selected items according to their category's display order. */
s32 item_sort_list_by_category_order(s32 category_index, s16* list) {
    s16 sorted_items[144];
    u8* category_order;
    s32 category_item_count;
    s32 sorted_count;
    s32 category_order_index;

    category_order = g_item_category_lists[category_index];
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

/* Reconcile one sorted item category list (0 weapons and shields, then
 * headgear, body armour, accessories and other: the menu category minus one)
 * with what the party owns: a newly owned item is put first, an item neither
 * owned nor equipped is removed, and the others keep their order. */
void item_rebuild_category_list(s32 category) {
    s32 i;
    s16 item;
    s32 type;
    s32 cat;

    for (i = 1; i < ITEM_ID_END; i++) {
        item = i;
        type = item_get_menu_category(item);
        cat = 0;
        if (type != ITEM_MENU_CATEGORY_WEAPON) {
            cat = type - 1;
        }
        if (category == cat) {
            if (item_get_total_count_variant(item) != 0) {
                if (item_is_in_category_list(i, category) == 0) {
                    item_prepend_to_category_list(i, category);
                }
            } else {
                item_remove_from_category_list(i, category);
            }
        }
    }
}

/* Returns whether an item occurs in a 0xFF-terminated category list. */
s32 item_is_in_category_list(s32 item_id, s32 category_index) {
    u8* item;

    for (item = g_item_category_lists[category_index]; *item != EQUIP_ITEM_LIST_END; item++) {
        if (*item == item_id) {
            return 1;
        }
    }
    return 0;
}

/* Prepend an item to the selected category list.
 *
 * The existing entries and their terminator move one byte to the right. */
void item_prepend_to_category_list(s32 item_id, s32 category_index) {
    u8* list;
    s32 i;

    list = g_item_category_lists[category_index];
    i = 0;
    while (list[i] != EQUIP_ITEM_LIST_END) {
        i++;
    }
    for (; i >= 0; i--) {
        list[i + 1] = list[i];
    }
    *list = item_id;
}

/* Remove the first matching item from a category list. */
void item_remove_from_category_list(s32 item_id, s32 category_index) {
    u8* list = g_item_category_lists[category_index];
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
