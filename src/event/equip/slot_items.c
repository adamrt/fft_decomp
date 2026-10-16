/* Slot items, equip checks, slot validation and the filtered item list. */

#include "fft/bits.h"
#include "fft/data.h"
#include "fft/event_equip.h"
#include "fft/item.h"
#include "fft/main.h"
#include "fft/main_item.h"
#include "fft/menu.h"
#include "psx/types.h"

static equip_item_slot_result_e equip_unit_validate_slot_item(s16 unit, s16 slot, s32 item_id);

/* Install an item into a unit's equipment slot. EQUIP copy of
 * world_formation_equip_item_to_unit_slot.
 *
 * Returns the validation result when it is negative. Otherwise the slot's
 * old item goes back to the inventory, and equipping into a hand slot with
 * any result other than EQUIP_SLOT_ITEM_ALLOWED also returns the other hand's
 * item and clears that slot. Unit index 1 is the preview record: it is
 * updated without touching the inventory or the battle stats. */
s32 equip_unit_set_slot_item(s16 unit_index, s16 slot, s32 item_id) {
    s16 value;
    equip_item_slot_result_e result;

    value = item_id & EQUIP_ITEM_ID_MASK;
    result = equip_unit_validate_slot_item(unit_index, slot, value);
    if (result < 0) {
        return result;
    }
    /* The typed equipment field folds 0x54 into the pointer expression and
     * changes register scheduling. This 16-bit view preserves the exact
     * target while indexing the same equip_unit_data_t.equipment array. */
    if (unit_index != 1) {
        item_adjust_inventory_count(((s16*)g_equip_unit_data[unit_index])[slot + 42], 1);
    }
    if (slot < EQUIP_SLOT_HAND_COUNT && result != EQUIP_SLOT_ITEM_ALLOWED) {
        s32 other = slot == EQUIP_SLOT_RIGHT_HAND;
        if (unit_index != 1) {
            item_adjust_inventory_count(((s16*)g_equip_unit_data[unit_index])[other + 42], 1);
        }
        ((s16*)g_equip_unit_data[unit_index])[other + 42] = 0;
    }
    ((s16*)g_equip_unit_data[unit_index])[slot + 42] = value;
    if (unit_index != 1) {
        equip_unit_commit_loadout_to_stats();
    }
    return EQUIP_SLOT_ITEM_ALLOWED;
}

/* Whether a unit's job can equip an item: EQUIP_SLOT_ITEM_ALLOWED for no item or an item whose
 * type bit is set in the unit's equippable-types bitfield, EQUIP_SLOT_ITEM_CANNOT_EQUIP for an item
 * of the "other" menu category or a type the unit cannot use. The low ten
 * bits of the entry are the item id. */
static s32 equip_unit_can_equip_item_id(s16 unit_index, s16 item_entry) {
    s32 item_id = item_entry & 0x3FF;
    s32 result;

    if (item_id == 0) {
        return EQUIP_SLOT_ITEM_ALLOWED;
    }
    if (item_get_menu_category(item_id) == ITEM_MENU_CATEGORY_OTHER) {
        return EQUIP_SLOT_ITEM_CANNOT_EQUIP;
    }
    bits_init_primary_reader_variant(g_equip_unit_data[unit_index]->equipment_categories);
    bits_read_primary(item_get_type(item_id));
    if (bits_read_primary(1) == 0) {
        result = EQUIP_SLOT_ITEM_CANNOT_EQUIP;
    } else {
        result = EQUIP_SLOT_ITEM_ALLOWED;
    }
    return result;
}

/* Validate a candidate item against slot locks, job rules, and hand pairing.
 *
 * The s16 slot parameter is retained because it reproduces the original
 * calling convention; equip_unit_equipment_slot_e names its accepted values.
 * The s16 id and its new_item copy reproduce the target's two callee-saved
 * copies of the masked id (the same shape as
 * world_formation_can_equip_item_in_slot). */
static equip_item_slot_result_e equip_unit_validate_slot_item(s16 unit, s16 slot, s32 item_id) {
    s32 result;
    s16 id;
    s16 new_item;
    s32 other;
    s32 other_slot;
    s32 is_nonstandard_weapon;
    s32 other_is_nonstandard_weapon;
    s32 forced_two_hands;
    s32 other_forced_two_hands;
    s32 two_swords;
    s32 other_two_swords;
    u8 flags;

    id = item_id & EQUIP_ITEM_ID_MASK;
    new_item = id;
    if (id == ITEM_ID_NOTHING) {
        return EQUIP_SLOT_ITEM_ALLOWED;
    }
    if (g_main_item_primary_data[g_equip_unit_data[unit]->equipment[slot]].type_flags & ITEM_TYPE_FLAG_UNREMOVABLE) {
        return EQUIP_SLOT_ITEM_CURRENT_LOCKED;
    }
    result = equip_unit_can_equip_item_id(unit, id);
    if (slot < EQUIP_SLOT_HAND_COUNT && result == EQUIP_SLOT_ITEM_ALLOWED) {
        is_nonstandard_weapon = id >= ITEM_ID_THROWABLE_FIRST;
        if (is_nonstandard_weapon) {
            forced_two_hands = 0;
            two_swords = 0;
        } else {
            flags = g_main_item_weapon_data[id].flags;
            forced_two_hands = flags & WEAPON_FLAG_FORCED_TWO_HANDS;
            two_swords = flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE;
        }
        other_slot = slot == EQUIP_SLOT_RIGHT_HAND;
        other = g_equip_unit_data[unit]->equipment[other_slot] & EQUIP_ITEM_ID_MASK;
        other_is_nonstandard_weapon = other >= ITEM_ID_THROWABLE_FIRST;
        if (other_is_nonstandard_weapon) {
            other_forced_two_hands = 0;
            other_two_swords = 0;
        } else {
            flags = g_main_item_weapon_data[other].flags;
            other_forced_two_hands = flags & WEAPON_FLAG_FORCED_TWO_HANDS;
            other_two_swords = flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE;
        }

        if (two_swords && other_two_swords && equip_unit_has_two_swords(unit)) {
            return EQUIP_SLOT_ITEM_ALLOWED;
        }
        if (!is_nonstandard_weapon && !other_is_nonstandard_weapon && new_item != ITEM_ID_NOTHING
            && other != ITEM_ID_NOTHING) {
            return EQUIP_SLOT_ITEM_INCOMPATIBLE_PAIR;
        }
        if (is_nonstandard_weapon && other_is_nonstandard_weapon) {
            return EQUIP_SLOT_ITEM_INCOMPATIBLE_PAIR;
        }
        if (is_nonstandard_weapon && other_forced_two_hands) {
            return EQUIP_SLOT_ITEM_INCOMPATIBLE_PAIR;
        }
        if (other_is_nonstandard_weapon && forced_two_hands) {
            return EQUIP_SLOT_ITEM_INCOMPATIBLE_PAIR;
        }
    }
    return result;
}

/* Builds the -1-terminated list of owned item IDs in `category`, then sorts it.
 *
 * `equip_filter` 0 lists every owned item, 1 flags items the unit cannot equip
 * with EQUIP_ITEM_LIST_ENTRY_UNEQUIPPABLE, and 2 omits them; filter 1 is
 * promoted to 2 when Show Unequippable is not On. `sort_mode` 1-5 with a
 * category list index below 5 uses that category order list; anything else falls back to
 * item_sort_id_list_variant. Returns the number of listed items.
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
