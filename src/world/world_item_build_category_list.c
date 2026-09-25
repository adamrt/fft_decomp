#include "fft/world.h"
#include "psx/types.h"

/*
 * Build the -1-terminated item list for an item menu category and sort it.
 *
 * Items 1-253 are listed when held (mode 3 counts inventory only, others
 * include equipped copies) and their type fits the category. Mode 1 flags
 * items the unit cannot equip with 0x4000, or hides them like mode 2 when Show
 * Unequippable is not On. Categories 0-4 and 7 with sort modes 1-5 go
 * through world_item_filter_sorted_entries, which returns the new count.
 */
s32 world_item_build_category_list(s16 unit_id, u16 sort_mode, s8 category, world_item_list_entry_t* entries, u8 mode) {
    s32 count;
    s32 item;
    s32 entry;
    s32 type;
    s32 result;
    s32 list_index;

    if (mode == 1 && g_main_game_options.fields.show_unequippable_items != GAME_OPTION_ON) {
        mode = 2;
    }
    item = 1;
    count = 0;
    for (; item < 0xFE; item++) {
        entry = item;
        if (mode == 3) {
            result = world_item_change_quantity_on_equip((s16)item, 0);
        } else {
            result = world_item_count_owned_and_equipped((s16)item);
        }
        if (result == 0) {
            continue;
        }
        type = world_item_get_menu_category((s16)entry);
        result = 0;
        if (category < 2) {
            result = type < 2;
        } else if (category == 2) {
            if (type == category) {
                result = 1;
            }
        } else if (category == 3) {
            if (type == category) {
                result = 1;
            }
        } else if (category == 4) {
            if (type == category) {
                result = 1;
            }
        } else if (category == 5) {
            if (type == 0) {
                result = 1;
            }
        } else if (category == 6) {
            if (type == 1) {
                result = 1;
            }
        } else if (category == 7) {
            if (type == 5) {
                result = 1;
            }
        }
        if (result == 0) {
            continue;
        }
        if (mode != 0) {
            result = world_unit_can_equip_item_id(unit_id, (s16)entry);
            if (mode >= 2 && result == -1) {
                continue;
            }
            if (mode == 1 && result == -1) {
                entry |= 0x4000;
            }
        }
        entries[count].value = entry;
        count++;
    }
    entries[count].value = -1;
    list_index = 100;
    if (category == 0) {
        list_index = 0;
    } else if (category < 5) {
        list_index = category - 1;
    } else if (category == 7) {
        list_index = 4;
    }
    if (list_index < 5 && (u16)(sort_mode - 1) < 5) {
        count = world_item_filter_sorted_entries(list_index, entries);
    } else {
        world_item_sort_id_list(sort_mode & 0xF, entries);
    }
    return count;
}
