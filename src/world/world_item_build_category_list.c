#include "fft/world.h"
#include "psx/types.h"

/*
 * Build the -1-terminated item list for an item menu category and sort it.
 *
 * Items 1-253 are listed when held (mode 3 counts inventory only, others
 * include equipped copies) and their type fits the category. Mode 1 flags
 * items the unit cannot equip with 0x4000, or hides them like mode 2 when Show
 * Unequippable is not On. Categories 0-4 and 7 with sort modes 1-5 go
 * through world_item_sort_list_by_category_order, which returns the new count.
 */
s32 world_item_build_category_list(s16 unit_id, s16 sort_mode, s8 category, s16* entries, u8 mode) {
    s32 count;
    s32 item;
    s32 entry;
    s32 type;
    s32 result;
    s32 list_index;

    if (mode == ITEM_LIST_MODE_MARK_UNEQUIPPABLE
        && g_main_game_options.fields.show_unequippable_items != GAME_OPTION_ON) {
        mode = ITEM_LIST_MODE_EQUIPPABLE;
    }
    item = 1;
    count = 0;
    for (; item < ITEM_ID_END; item++) {
        entry = item;
        if (mode == ITEM_LIST_MODE_EQUIPPABLE_IN_INVENTORY) {
            result = item_adjust_inventory_count(item, 0);
        } else {
            result = item_get_total_count_variant(item);
        }
        if (result == 0) {
            continue;
        }
        type = item_get_menu_category(entry);
        result = 0;
        if (category < 2) {
            result = type < 2;
        } else if (category == ITEM_LIST_FILTER_HEADGEAR) {
            if (type == category) {
                result = 1;
            }
        } else if (category == ITEM_LIST_FILTER_BODY_ARMOR) {
            if (type == category) {
                result = 1;
            }
        } else if (category == ITEM_LIST_FILTER_ACCESSORY) {
            if (type == category) {
                result = 1;
            }
        } else if (category == ITEM_LIST_FILTER_WEAPON) {
            if (type == ITEM_MENU_CATEGORY_WEAPON) {
                result = 1;
            }
        } else if (category == ITEM_LIST_FILTER_SHIELD) {
            if (type == ITEM_MENU_CATEGORY_SHIELD) {
                result = 1;
            }
        } else if (category == ITEM_LIST_FILTER_OTHER) {
            if (type == ITEM_MENU_CATEGORY_OTHER) {
                result = 1;
            }
        }
        if (result == 0) {
            continue;
        }
        if (mode != ITEM_LIST_MODE_ALL) {
            result = world_unit_can_equip_item_id(unit_id, entry);
            if (mode >= 2 && result == -1) {
                continue;
            }
            if (mode == ITEM_LIST_MODE_MARK_UNEQUIPPABLE && result == -1) {
                entry |= WORLD_ITEM_LIST_ENTRY_UNEQUIPPABLE;
            }
        }
        entries[count] = entry;
        count++;
    }
    entries[count] = -1;
    list_index = 100;
    if (category == ITEM_LIST_FILTER_HAND) {
        list_index = 0;
    } else if (category < 5) {
        list_index = category - 1;
    } else if (category == ITEM_LIST_FILTER_OTHER) {
        list_index = 4;
    }
    if (list_index < 5 && sort_mode >= 1 && sort_mode < 6) {
        count = item_sort_list_by_category_order(list_index, entries);
    } else {
        world_item_sort_id_list(sort_mode & 0xF, entries);
    }
    return count;
}
