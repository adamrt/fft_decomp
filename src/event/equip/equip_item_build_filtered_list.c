#include "fft/event_equip.h"

/* Builds the -1-terminated list of owned item IDs in `category`, then sorts it.
 *
 * `equip_filter` 0 lists every owned item, 1 flags items the unit cannot equip
 * with EQUIP_ITEM_LIST_ENTRY_UNEQUIPPABLE, and 2 omits them; filter 1 is
 * promoted to 2 when Show Unequippable is not On. `sort_mode` 1-5 with a
 * category list index below 5 uses that category order list; anything else falls back to
 * equip_item_sort_list_by_criteria. Returns the number of listed items.
 */
s32 equip_item_build_filtered_list(s16 unit_index, s16 sort_mode, s8 category, s16* list, u8 equip_filter) {
    s32 count;
    s32 item_id;
    s32 entry;
    s32 item_category;
    s32 matches;
    s32 category_index;

    if (equip_filter == ITEM_LIST_MODE_MARK_UNEQUIPPABLE
        && g_main_game_options.fields.show_unequippable_items != GAME_OPTION_ON) {
        equip_filter = ITEM_LIST_MODE_EQUIPPABLE;
    }
    item_id = 1;
    count = 0;
    for (; item_id < ITEM_ID_END; item_id++) {
        entry = item_id;
        if (item_get_total_count_variant(item_id) == 0) {
            continue;
        }
        item_category = item_get_menu_category(item_id);
        matches = 0;
        if (category < 2) {
            matches = item_category < 2;
        } else if (category == ITEM_LIST_FILTER_HEADGEAR) {
            if (item_category == ITEM_MENU_CATEGORY_HEADGEAR) {
                matches = 1;
            }
        } else if (category == ITEM_LIST_FILTER_BODY_ARMOR) {
            if (item_category == category) {
                matches = 1;
            }
        } else if (category == ITEM_LIST_FILTER_ACCESSORY) {
            if (item_category == category) {
                matches = 1;
            }
        } else if (category == ITEM_LIST_FILTER_WEAPON) {
            if (item_category == ITEM_MENU_CATEGORY_WEAPON) {
                matches = 1;
            }
        } else if (category == ITEM_LIST_FILTER_SHIELD) {
            if (item_category == ITEM_MENU_CATEGORY_SHIELD) {
                matches = 1;
            }
        } else if (category == ITEM_LIST_FILTER_OTHER) {
            if (item_category == ITEM_MENU_CATEGORY_OTHER) {
                matches = 1;
            }
        }
        if (matches == 0) {
            continue;
        }
        if (equip_filter != ITEM_LIST_MODE_ALL) {
            matches = equip_unit_can_equip_item_id(unit_index, entry);
            if (equip_filter == ITEM_LIST_MODE_EQUIPPABLE && matches == -1) {
                continue;
            }
            if (equip_filter == ITEM_LIST_MODE_MARK_UNEQUIPPABLE && matches == -1) {
                entry |= EQUIP_ITEM_LIST_ENTRY_UNEQUIPPABLE;
            }
        }
        list[count] = entry;
        count++;
    }
    list[count] = -1;

    category_index = 100;
    if (category == ITEM_LIST_FILTER_HAND) {
        category_index = 0;
    } else if (category < 5) {
        category_index = category - 1;
    } else if (category == ITEM_LIST_FILTER_OTHER) {
        category_index = 4;
    }
    if (category_index < 5 && sort_mode >= 1 && sort_mode < 6) {
        count = item_sort_list_by_category_order(category_index, list);
    } else {
        item_sort_id_list_variant(sort_mode & 0xF, list);
    }
    return count;
}
