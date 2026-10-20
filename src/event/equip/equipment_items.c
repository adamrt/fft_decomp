/* Equipment slot changes, item eligibility and available item counts. */

#include "fft/ability.h"
#include "fft/bits.h"
#include "fft/equipment.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/option.h"
#include "psx/types.h"

static equipment_slot_result_e equipment_validate_slot_item(s16 unit, s16 slot, s32 item_id);

/* Number of copies of the item the party has equipped: the total held
 * (g_equipment_item_inventory_totals) minus the inventory count (read with a zero delta). */
s32 item_equipped_count_get(s32 item_entry) {
    s32 item_id = item_entry & ITEM_ENTRY_ID_MASK;

    return g_equipment_item_inventory_totals[item_id] - item_inventory_count_adjust(item_id, 0);
}

/* Return how many of an item the party has in all, from the inventory totals table; only the
 * low ten bits of item_id are used. */
s32 item_total_count_get(s16 item_id) {
    return g_equipment_item_inventory_totals[item_id & ITEM_ENTRY_ID_MASK];
}

/* Install an item into a unit's equipment slot.
 *
 * Returns the validation result when it is negative. Otherwise the slot's
 * old item goes back to the inventory, and equipping into a hand slot with
 * any result other than EQUIPMENT_SLOT_ITEM_ALLOWED also returns the other hand's
 * item and clears that slot. Unit index 1 is the preview record: it is
 * updated without touching the inventory or the battle stats. */
s32 equipment_set_slot_item(s16 unit_index, s16 slot, s32 item_id) {
    s16 value = item_id & ITEM_ENTRY_ID_MASK;
    equipment_slot_result_e result = equipment_validate_slot_item(unit_index, slot, value);

    if (result < 0) {
        return result;
    }
    if (unit_index != 1) {
        item_inventory_count_adjust(g_equipment_unit_data[unit_index]->equipment[slot], 1);
    }
    if (slot < MENU_EQUIPMENT_SLOT_HAND_COUNT && result != EQUIPMENT_SLOT_ITEM_ALLOWED) {
        s32 other = slot == MENU_EQUIPMENT_SLOT_RIGHT_HAND;
        if (unit_index != 1) {
            item_inventory_count_adjust(g_equipment_unit_data[unit_index]->equipment[other], 1);
        }
        g_equipment_unit_data[unit_index]->equipment[other] = 0;
    }
    g_equipment_unit_data[unit_index]->equipment[slot] = value;
    if (unit_index != 1) {
        equipment_commit_loadout();
    }
    return EQUIPMENT_SLOT_ITEM_ALLOWED;
}

/* Whether a unit's job can equip an item: EQUIPMENT_SLOT_ITEM_ALLOWED for no item or an item whose
 * type bit is set in the unit's equippable-types bitfield, EQUIPMENT_SLOT_ITEM_CANNOT_EQUIP for an item
 * of the "other" menu category or a type the unit cannot use. The low ten
 * bits of the entry are the item id. */
s32 equipment_item_compatibility_check(s16 unit_index, s16 item_entry) {
    s32 item_id = item_entry & ITEM_ENTRY_ID_MASK;

    if (item_id == 0) {
        return EQUIPMENT_SLOT_ITEM_ALLOWED;
    }
    if (item_menu_category_get(item_id) == ITEM_MENU_CATEGORY_OTHER) {
        return EQUIPMENT_SLOT_ITEM_CANNOT_EQUIP;
    }
    /* Skip to the item type's bit in the unit's bitfield, first byte's bit 7 first. */
    bits_primary_reader_init(g_equipment_unit_data[unit_index]->equipment_categories);
    bits_primary_read(item_type_get(item_id));
    return bits_primary_read(1) == 0 ? EQUIPMENT_SLOT_ITEM_CANNOT_EQUIP : EQUIPMENT_SLOT_ITEM_ALLOWED;
}

/* Validate a candidate item against slot locks, job rules, and hand pairing.
 *
 * The s16 slot parameter is retained because it reproduces the original
 * calling convention; menu_equipment_slot_e names its accepted values.
 * The s16 id and its new_item copy reproduce the target's two callee-saved
 * copies of the masked id (the same shape as
 * formation_item_slot_check). */
static equipment_slot_result_e equipment_validate_slot_item(s16 unit, s16 slot, s32 item_id) {
    s32 result;
    s16 id = item_id & ITEM_ENTRY_ID_MASK;
    s16 new_item = id;
    s32 other;
    s32 other_slot;
    s32 is_nonstandard_weapon;
    s32 other_is_nonstandard_weapon;
    s32 forced_two_hands;
    s32 other_forced_two_hands;
    s32 two_swords;
    s32 other_two_swords;
    u8 flags;

    if (id == ITEM_ID_NOTHING) {
        return EQUIPMENT_SLOT_ITEM_ALLOWED;
    }
    if (g_item_primary_data[g_equipment_unit_data[unit]->equipment[slot]].type_flags & ITEM_TYPE_FLAG_UNREMOVABLE) {
        return EQUIPMENT_SLOT_ITEM_CURRENT_LOCKED;
    }
    result = equipment_item_compatibility_check(unit, id);

    /* Hand pairing, where ids from ITEM_ID_THROWABLE_FIRST on (shields included) count as
     * non-weapons: two Two Swords weapons pair when the unit has Two Swords; otherwise two
     * weapons, two non-weapons, or a non-weapon beside a forced two-hand weapon do not. */
    if (slot < MENU_EQUIPMENT_SLOT_HAND_COUNT && result == EQUIPMENT_SLOT_ITEM_ALLOWED) {
        is_nonstandard_weapon = id >= ITEM_ID_THROWABLE_FIRST;
        if (is_nonstandard_weapon) {
            forced_two_hands = 0;
            two_swords = 0;
        } else {
            flags = g_item_weapon_data[id].flags;
            forced_two_hands = flags & WEAPON_FLAG_FORCED_TWO_HANDS;
            two_swords = flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE;
        }
        other_slot = slot == MENU_EQUIPMENT_SLOT_RIGHT_HAND;
        other = g_equipment_unit_data[unit]->equipment[other_slot] & ITEM_ENTRY_ID_MASK;
        other_is_nonstandard_weapon = other >= ITEM_ID_THROWABLE_FIRST;
        if (other_is_nonstandard_weapon) {
            other_forced_two_hands = 0;
            other_two_swords = 0;
        } else {
            flags = g_item_weapon_data[other].flags;
            other_forced_two_hands = flags & WEAPON_FLAG_FORCED_TWO_HANDS;
            other_two_swords = flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE;
        }

        if (two_swords && other_two_swords && ability_has_two_swords(unit)) {
            return EQUIPMENT_SLOT_ITEM_ALLOWED;
        }
        if (!is_nonstandard_weapon && !other_is_nonstandard_weapon && new_item != ITEM_ID_NOTHING
            && other != ITEM_ID_NOTHING) {
            return EQUIPMENT_SLOT_ITEM_INCOMPATIBLE_PAIR;
        }
        if (is_nonstandard_weapon && other_is_nonstandard_weapon) {
            return EQUIPMENT_SLOT_ITEM_INCOMPATIBLE_PAIR;
        }
        if (is_nonstandard_weapon && other_forced_two_hands) {
            return EQUIPMENT_SLOT_ITEM_INCOMPATIBLE_PAIR;
        }
        if (other_is_nonstandard_weapon && forced_two_hands) {
            return EQUIPMENT_SLOT_ITEM_INCOMPATIBLE_PAIR;
        }
    }
    return result;
}

/* Builds the -1-terminated list of owned item IDs in `category`, then sorts it.
 *
 * `equip_filter` 0 lists every owned item, 1 flags items the unit cannot equip
 * with ITEM_ENTRY_UNEQUIPPABLE, and 2 omits them; filter 1 is
 * promoted to 2 when Show Unequippable is not On. `sort_mode` 1-5 with a
 * category list index below 5 uses that category order list; anything else falls back to
 * item_id_list_sort. Returns the number of listed items.
 */
s32 equipment_build_item_list(s16 unit_index, s16 sort_mode, s8 category, s16* list, u8 equip_filter) {
    s32 count;
    s32 item_id;
    s32 entry;
    s32 item_category;
    s32 matches;
    s32 category_index;

    if (equip_filter == ITEM_LIST_MODE_MARK_UNEQUIPPABLE
        && g_game_options.fields.show_unequippable_items != GAME_OPTION_ON) {
        equip_filter = ITEM_LIST_MODE_EQUIPPABLE;
    }
    item_id = 1;
    count = 0;
    for (; item_id < ITEM_ID_END; item_id++) {
        entry = item_id;
        if (item_total_count_get(item_id) == 0) {
            continue;
        }
        item_category = item_menu_category_get(item_id);
        matches = 0;
        if (category < ITEM_LIST_FILTER_HEADGEAR) {
            /* Either hand lists weapons and shields. */
            matches = item_category < ITEM_MENU_CATEGORY_HEADGEAR;
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
            matches = equipment_item_compatibility_check(unit_index, entry);
            if (equip_filter == ITEM_LIST_MODE_EQUIPPABLE && matches == EQUIPMENT_SLOT_ITEM_CANNOT_EQUIP) {
                continue;
            }
            if (equip_filter == ITEM_LIST_MODE_MARK_UNEQUIPPABLE && matches == EQUIPMENT_SLOT_ITEM_CANNOT_EQUIP) {
                entry |= ITEM_ENTRY_UNEQUIPPABLE;
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
        item_id_list_sort(sort_mode & 0xF, list);
    }
    return count;
}
