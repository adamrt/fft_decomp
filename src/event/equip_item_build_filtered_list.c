#include "fft/event_equip.h"

/* Builds the -1-terminated list of owned item IDs in `category`, then sorts it.
 *
 * `equip_filter` 0 lists every owned item, 1 flags items the unit cannot equip
 * with 0x4000, and 2 omits them; filter 1 is promoted to 2 when Show
 * Unequippable is not On. `sort_mode` 1-5 with a category
 * key below 5 uses the category order list; anything else falls back to
 * equip_item_sort_list_by_criteria. Returns the number of listed items.
 */
s32 equip_item_build_filtered_list(s16 unit_index, u16 sort_mode, s8 category, s16* list, u8 equip_filter) {
    s32 count;
    s32 item_id;
    s32 entry;
    s32 item_category;
    s32 matches;
    s32 sort_key;

    if (equip_filter == 1 && g_main_game_options.fields.show_unequippable_items != GAME_OPTION_ON) {
        equip_filter = 2;
    }
    item_id = 1;
    count = 0;
    for (; item_id < 0xFE; item_id++) {
        entry = item_id;
        if (equip_item_get_total_count((s16)item_id) == 0) {
            continue;
        }
        item_category = equip_item_get_category((s16)item_id);
        matches = 0;
        if (category < 2) {
            matches = item_category < 2;
        } else if (category == 2) {
            if (item_category == 2) {
                matches = 1;
            }
        } else if (category == 3) {
            if (item_category == category) {
                matches = 1;
            }
        } else if (category == 4) {
            if (item_category == category) {
                matches = 1;
            }
        } else if (category == 5) {
            if (item_category == 0) {
                matches = 1;
            }
        } else if (category == 6) {
            if (item_category == 1) {
                matches = 1;
            }
        } else if (category == 7) {
            if (item_category == 5) {
                matches = 1;
            }
        }
        if (matches == 0) {
            continue;
        }
        if (equip_filter != 0) {
            matches = equip_unit_can_equip_item(unit_index, (s16)entry);
            if (equip_filter == 2 && matches == -1) {
                continue;
            }
            if (equip_filter == 1 && matches == -1) {
                entry |= 0x4000; /* listed but not equippable */
            }
        }
        list[count] = entry;
        count++;
    }
    list[count] = -1;

    sort_key = 100;
    if (category == 0) {
        sort_key = 0;
    } else if (category < 5) {
        sort_key = category - 1;
    } else if (category == 7) {
        sort_key = 4;
    }
    if (sort_key < 5 && (u16)(sort_mode - 1) < 5) {
        count = equip_item_sort_list_by_category_order(sort_key, list);
    } else {
        equip_item_sort_list_by_criteria(sort_mode & 0xF, list);
    }
    return count;
}
