/* Item availability, prices and counts, poached item quantities, best fit equipment, equipping items to slots, equip
 * checks, job changes, loadout validation and item category lists. */

#include "fft/ability.h"
#include "fft/bits.h"
#include "fft/equipment.h"
#include "fft/formation.h"
#include "fft/formation_ability.h"
#include "fft/formation_item.h"
#include "fft/formation_job.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/menu.h"
#include "fft/option.h"
#include "fft/shop.h"
#include "fft/unit.h"
#include "psx/libc.h"
#include "psx/types.h"

static u8 g_formation_item_best_fit_slot_categories[MENU_EQUIPMENT_SLOT_COUNT] = { ITEM_LIST_FILTER_WEAPON,
    ITEM_LIST_FILTER_SHIELD, ITEM_LIST_FILTER_HEADGEAR, ITEM_LIST_FILTER_BODY_ARMOR, ITEM_LIST_FILTER_ACCESSORY };
static u8 g_shop_fitting_best_fit_slot_categories[MENU_EQUIPMENT_SLOT_COUNT] = { ITEM_LIST_FILTER_WEAPON,
    ITEM_LIST_FILTER_SHIELD, ITEM_LIST_FILTER_HEADGEAR, ITEM_LIST_FILTER_BODY_ARMOR, ITEM_LIST_FILTER_ACCESSORY };

/* Report whether the party has any items: mode 0 asks for inventory or equipped items, mode 1
 * for inventory only, other modes for equipped items only.
 */
s32 formation_has_items(s32 mode) {
    s32 has_inventory = 0;
    s32 has_equipment = 0;
    s32 i;
    s32 slot;

    for (i = 1; i < ITEM_ID_END; i++) {
        if (g_item_quantities[i]) {
            has_inventory = 1;
            break;
        }
    }
    for (i = 0; i < g_formation_record_count; i++) {
        for (slot = 0; slot < MENU_EQUIPMENT_SLOT_COUNT; slot++) {
            if (g_formation_unit_pointers[i]->equipment[slot]) {
                has_equipment = 1;
                break;
            }
        }
    }
    if (mode == 0) {
        return has_equipment || has_inventory;
    }
    if (mode == 1) {
        return has_inventory;
    }
    return has_equipment;
}

/* Return an item's price from the item data table; only the item id bits of the entry are used. */
s32 shop_get_item_price(s16 item_id) {
    return g_item_primary_data[item_id & ITEM_ENTRY_ID_MASK].price;
}

/* Return half of the item-table price, clamped to at least 1. */
s32 shop_get_item_half_price(s16 item_id) {
    u32 value = g_item_primary_data[item_id & ITEM_ENTRY_ID_MASK].price >> 1;

    if (value == 0) {
        value = 1;
    }
    return value;
}

/* Count copies of an item equipped across the active formation records.
 *
 * The comparison uses the stored 16-bit equipment value as-is; only the requested
 * ID is masked. */
s32 item_get_equipped_count(s32 item_id) {
    s32 count = 0;
    s32 unit_index;
    s32 slot;
    formation_unit_t* unit;

    item_id &= ITEM_ENTRY_ID_MASK;
    for (unit_index = 0; unit_index < g_formation_record_count; unit_index++) {
        unit = g_formation_unit_pointers[unit_index];
        for (slot = 0; slot < MENU_EQUIPMENT_SLOT_COUNT; slot++) {
            if (unit->equipment[slot] == item_id) {
                count++;
            }
        }
        /* The target reloads the record count on every pass although nothing
         * in the loop stores; without this the load is hoisted. */
        __asm__("" : : : "memory");
    }
    return count;
}

/* Inventory quantity plus the copies equipped by the party. */
s32 item_get_total_count(s16 item_id) {
    item_id &= ITEM_ENTRY_ID_MASK;
    return item_adjust_inventory_count(item_id, 0) + item_get_equipped_count(item_id);
}

/* Add delta to the count of a poached item held for the Fur Shop, keeping it within 0..255, and
 * return the new count; callers pass 0 to read it. */
s32 fur_shop_adjust_stock(s16 item_id, s32 delta) {
    s32 slot = item_id & ITEM_ENTRY_ID_MASK;
    s32 value = g_item_poached_quantities[slot] + delta;

    if (value < 0) {
        value = 0;
    }
    if (value >= 0x100) {
        value = 0xFF;
    }
    g_item_poached_quantities[slot] = value;
    return value;
}

/* Choose the best-ranked owned items for each equipment slot into `equipment`, keeping
 * compatible hand combinations and swapping hands when needed. Martial Arts clears both.
 *
 * Each slot's candidates are the equippable items in the inventory plus what the unit holds
 * there, sorted by the best-fit key list. Two Hands prefers a weapon that can be held
 * two-handed and then leaves the off hand empty; Two Swords (without Two Hands) fills the off
 * hand with a second weapon, the same one only when the unit has two copies.
 * shop_fitting_build_best_fit is the shop's sibling.
 */
void formation_item_build_best_fit(s16 unit_index, s16* equipment) {
    s16 list[0x90];
    s32 right_hand;
    s16 two_swords;
    s32 left_hand;
    s32 held_weapon;
    s32 held_off_hand;
    s32 slot;
    s32 category;
    s16 count;
    s16 length;
    s32 i;
    s16 owned;
    s16 retry;

    two_swords = 0;
    right_hand = g_formation_unit_pointers[unit_index]->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND];
    left_hand = g_formation_unit_pointers[unit_index]->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND];
    if (ability_has_two_swords(unit_index)) {
        two_swords = formation_ability_has_two_hands(unit_index) == 0;
    }
    held_off_hand = ITEM_ID_NOTHING;
    held_weapon = ITEM_ID_NOTHING;
    if (two_swords) {
        held_weapon = right_hand;
        g_formation_item_best_fit_slot_categories[MENU_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_LIST_FILTER_WEAPON;
        held_off_hand = left_hand;
        if (held_weapon == ITEM_ID_NOTHING) {
            held_weapon = left_hand;
            held_off_hand = ITEM_ID_NOTHING;
        }
    } else {
        g_formation_item_best_fit_slot_categories[MENU_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_LIST_FILTER_SHIELD;
        if (right_hand >= ITEM_ID_THROWABLE_FIRST)
            held_off_hand = right_hand;
        else if (right_hand != ITEM_ID_NOTHING)
            held_weapon = right_hand;
        if (left_hand >= ITEM_ID_THROWABLE_FIRST)
            held_off_hand = left_hand;
        else if (left_hand != ITEM_ID_NOTHING)
            held_weapon = left_hand;
    }

    for (slot = 0; slot < MENU_EQUIPMENT_SLOT_COUNT; slot++) {
        category = g_formation_item_best_fit_slot_categories[slot];
        count = formation_item_build_list(unit_index, ITEM_SORT_ORDER_BEST_FIT,
            g_formation_item_best_fit_slot_categories[slot], list, ITEM_LIST_MODE_EQUIPPABLE_IN_INVENTORY);
        length = count;
        if (category == ITEM_LIST_FILTER_WEAPON) {
            if (held_weapon < ITEM_ID_THROWABLE_FIRST)
                list[length++] = held_weapon;
            if (held_off_hand < ITEM_ID_THROWABLE_FIRST)
                list[length++] = held_off_hand;
            list[length] = -1;
            item_sort_id_list(ITEM_SORT_ORDER_BEST_FIT, list);
            if (formation_ability_has_two_hands(unit_index)) {
                for (i = 0; list[i] != -1; i++) {
                    s16 item_id = list[i];
                    if (g_item_weapon_data[item_id].flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE) {
                        list[0] = item_id;
                        break;
                    }
                }
            } else if (two_swords) {
                /* With no Two Swords weapon left, the off hand falls back to a shield and is picked again. */
                retry = 0;
                for (i = 0;; i++) {
                    if (list[i] == -1) {
                        g_formation_item_best_fit_slot_categories[MENU_EQUIPMENT_SLOT_LEFT_HAND]
                            = ITEM_LIST_FILTER_SHIELD;
                        if (slot == MENU_EQUIPMENT_SLOT_LEFT_HAND) {
                            retry = 1;
                            slot--;
                        }
                        break;
                    }
                    if (g_item_weapon_data[list[i]].flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE) {
                        if (slot == MENU_EQUIPMENT_SLOT_RIGHT_HAND) {
                            list[0] = list[i];
                            break;
                        }
                        owned = item_adjust_inventory_count(list[i], 0);
                        if (held_weapon == list[i])
                            owned++;
                        if (held_off_hand == list[i])
                            owned++;
                        if (owned >= 2 || equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND] != list[i]) {
                            list[0] = list[i];
                            break;
                        }
                    }
                }
                if (retry)
                    continue;
            }
        } else if (category == ITEM_LIST_FILTER_SHIELD) {
            if ((formation_ability_has_two_hands(unit_index)
                    && (g_item_weapon_data[equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND]].flags
                        & WEAPON_FLAG_TWO_HANDS_COMPATIBLE))
                || (g_item_weapon_data[equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND]].flags
                    & WEAPON_FLAG_FORCED_TWO_HANDS)) {
                list[0] = -1;
            } else {
                if (held_off_hand >= ITEM_ID_THROWABLE_FIRST)
                    list[length++] = held_off_hand;
                list[length] = -1;
                item_sort_id_list(ITEM_SORT_ORDER_BEST_FIT, list);
            }
        } else {
            list[length++] = g_formation_unit_pointers[unit_index]->equipment[slot];
            list[length] = -1;
            item_sort_id_list(ITEM_SORT_ORDER_BEST_FIT, list);
        }
        if (list[0] <= ITEM_ID_NOTHING)
            equipment[slot] = ITEM_ID_NOTHING;
        else
            equipment[slot] = list[0];
    }

    i = 0; /* reused as the swap flag (the target keeps it in i's register) */
    if (g_formation_unit_pointers[unit_index]->support_abilities[2] & UNIT_SUPPORT_SET_3_MARTIAL_ARTS) {
        equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND] = ITEM_ID_NOTHING;
        equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_ID_NOTHING;
    }

    /* Swap the picks when a held item changed hands, or the unit held its weapon left (without Two
     * Swords) or its off-hand item right. */
    if (right_hand != ITEM_ID_NOTHING && right_hand == equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND]) {
        i = 1;
    } else if (left_hand != ITEM_ID_NOTHING && left_hand == equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND]) {
        i = 1;
    } else if (!two_swords && left_hand < ITEM_ID_THROWABLE_FIRST && left_hand != ITEM_ID_NOTHING) {
        i = 1;
    } else if (right_hand >= ITEM_ID_THROWABLE_FIRST) {
        i = 1;
    }
    if (i) {
        s32 swap = equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND];
        equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND] = equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND];
        equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND] = swap;
    }
}

/* Choose fitting-room equipment through the body slot, then mark newly reserved
 * shop items. The accessory slot is left to the caller.
 */
void shop_fitting_build_best_fit(s16 unit_index, s16 shop_id, s16* out) {
    s16 list[0x90];
    s32 right;
    /* Declaration order decides the reload slots: the target spills `first` to
     * sp+328 and `dual` to sp+336, so `first` must be declared first. */
    s32 first;
    s32 left;
    s16 dual;
    s32 second;
    s32 slot;
    s32 category;
    s16 count;
    s16 length;
    s32 i;
    s16 retry;
    s16 reserved;

    reserved = g_shop_fitting_items[unit_index][MENU_EQUIPMENT_SLOT_RIGHT_HAND];
    if (reserved != ITEM_ID_NOTHING) {
        right = reserved;
    } else {
        right = g_formation_unit_pointers[unit_index]->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND] & ITEM_ENTRY_ID_MASK;
    }
    reserved = g_shop_fitting_items[unit_index][MENU_EQUIPMENT_SLOT_LEFT_HAND];
    if (reserved != ITEM_ID_NOTHING) {
        left = reserved;
    } else {
        left = g_formation_unit_pointers[unit_index]->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND] & ITEM_ENTRY_ID_MASK;
    }

    /* Two Hands takes precedence over Two Swords. */
    second = ITEM_ID_NOTHING;
    first = ITEM_ID_NOTHING;
    dual = 0;
    if (ability_has_two_swords(unit_index)) {
        dual = formation_ability_has_two_hands(unit_index) == 0;
    }

    if (dual) {
        second = left;
        g_shop_fitting_best_fit_slot_categories[MENU_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_LIST_FILTER_WEAPON;
        /* Written last so it is the instruction immediately before the test:
         * reorg scans backward from the branch and takes it into the delay
         * slot, which is where the target has `sw s8,328(sp)`. */
        first = right;
        if (first == ITEM_ID_NOTHING) {
            first = left;
            second = ITEM_ID_NOTHING;
        }
    } else {
        g_shop_fitting_best_fit_slot_categories[MENU_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_LIST_FILTER_SHIELD;
        if (right >= ITEM_ID_THROWABLE_FIRST)
            second = right;
        else if (right != ITEM_ID_NOTHING)
            first = right;
        if (left >= ITEM_ID_THROWABLE_FIRST)
            second = left;
        else if (left != ITEM_ID_NOTHING)
            first = left;
    }

    for (slot = 0; slot < MENU_EQUIPMENT_SLOT_ACCESSORY; slot++) {
        category = g_shop_fitting_best_fit_slot_categories[slot];
        count = shop_build_stock_list(unit_index, shop_id, category, list, ITEM_LIST_MODE_EQUIPPABLE);
        length = count;

        if (category == ITEM_LIST_FILTER_WEAPON) {
            /* One lvalue per arm: the target computes &list[length] once and stores
             * through it from both sides of the test (`sh t0,0(v1)` then
             * `sh zero,0(a0)`), with `length` post-incremented. Two separate
             * `list[length] = ...` statements duplicate the address computation. */
            if (slot == MENU_EQUIPMENT_SLOT_RIGHT_HAND) {
                list[length++] = (first < ITEM_ID_THROWABLE_FIRST) ? first : ITEM_ID_NOTHING;
            }
            if (slot == MENU_EQUIPMENT_SLOT_LEFT_HAND) {
                list[length++] = (second < ITEM_ID_THROWABLE_FIRST) ? second : ITEM_ID_NOTHING;
            }
            list[length] = -1;
            item_sort_id_list(ITEM_SORT_ORDER_BEST_FIT, list);
            if (formation_ability_has_two_hands(unit_index)) {
                for (i = 0; list[i] != -1; i++) {
                    s16 item_id = list[i];
                    if (g_item_weapon_data[item_id].flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE) {
                        list[0] = item_id;
                        break;
                    }
                }
            } else if (dual) {
                /* With no Two Swords weapon left, the off hand falls back to a shield and is picked again. */
                retry = 0;
                for (i = 0;; i++) {
                    if (list[i] == -1) {
                        g_shop_fitting_best_fit_slot_categories[MENU_EQUIPMENT_SLOT_LEFT_HAND]
                            = ITEM_LIST_FILTER_SHIELD;
                        if (slot == MENU_EQUIPMENT_SLOT_LEFT_HAND) {
                            retry = 1;
                            slot--;
                        }
                        break;
                    }
                    if (g_item_weapon_data[list[i]].flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE) {
                        list[0] = list[i];
                        break;
                    }
                }
                if (retry)
                    continue;
            }
        } else if (category == ITEM_LIST_FILTER_SHIELD) {
            if ((formation_ability_has_two_hands(unit_index)
                    && (g_item_weapon_data[out[MENU_EQUIPMENT_SLOT_RIGHT_HAND]].flags
                        & WEAPON_FLAG_TWO_HANDS_COMPATIBLE))
                || (g_item_weapon_data[out[MENU_EQUIPMENT_SLOT_RIGHT_HAND]].flags & WEAPON_FLAG_FORCED_TWO_HANDS)) {
                list[0] = -1;
            } else {
                if (second >= ITEM_ID_THROWABLE_FIRST)
                    list[length++] = second;
                list[length] = -1;
                item_sort_id_list(ITEM_SORT_ORDER_BEST_FIT, list);
            }
        } else {
            list[length++] = g_formation_unit_pointers[unit_index]->equipment[slot] & ITEM_ENTRY_ID_MASK;
            list[length] = -1;
            item_sort_id_list(ITEM_SORT_ORDER_BEST_FIT, list);
        }

        if (list[0] <= ITEM_ID_NOTHING)
            out[slot] = ITEM_ID_NOTHING;
        else
            out[slot] = list[0];
    }

    i = 0; /* reused as the swap flag, as in the sibling */
    if (g_formation_unit_pointers[unit_index]->support_abilities[2] & UNIT_SUPPORT_SET_3_MARTIAL_ARTS) {
        out[MENU_EQUIPMENT_SLOT_RIGHT_HAND] = ITEM_ID_NOTHING;
        out[MENU_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_ID_NOTHING;
    }

    /* Swap the picks when a held item changed hands, or the unit held its weapon left (without Two
     * Swords) or its off-hand item right. */
    if ((right != ITEM_ID_NOTHING && right == out[MENU_EQUIPMENT_SLOT_LEFT_HAND])
        || (left != ITEM_ID_NOTHING && left == out[MENU_EQUIPMENT_SLOT_RIGHT_HAND])
        || (!dual && left < ITEM_ID_THROWABLE_FIRST && left != ITEM_ID_NOTHING) || right >= ITEM_ID_THROWABLE_FIRST)
        i = 1;
    if (i) {
        /* Reusing the swap flag preserves the target's register assignment. */
        i = out[MENU_EQUIPMENT_SLOT_RIGHT_HAND];
        out[MENU_EQUIPMENT_SLOT_RIGHT_HAND] = out[MENU_EQUIPMENT_SLOT_LEFT_HAND];
        out[MENU_EQUIPMENT_SLOT_LEFT_HAND] = i;
    }

    /* A pick the unit neither wears nor has reserved must come from the shop. */
    for (slot = 0; slot < MENU_EQUIPMENT_SLOT_ACCESSORY; slot++) {
        s16 item = out[slot];
        if (item != g_formation_unit_pointers[unit_index]->equipment[slot]
            && item != g_shop_fitting_items[unit_index][slot]) {
            out[slot] = item | SHOP_FITTING_ITEM;
        }
    }
}

/* Install an item into a formation unit's equipment slot and return 1, or the
 * negative formation_item_can_equip_in_slot result.
 *
 * For a roster unit the slot's old item goes back to the inventory and the
 * records are saved to the party; the preview unit only has its record
 * changed. When the item cannot pair with the other hand's item, that item is
 * returned the same way and its slot cleared.
 *
 * `item` is a separate u16 mirror of the masked argument: one s32 variable
 * reused for both the call argument and the stored value is copy-propagated and
 * swaps which of the two ends up in the callee-saved register.
 */
s32 formation_item_equip(s16 unit, s16 slot, s16 item_id) {
    s32 status;
    s32 other;
    u16 item;

    item_id &= ITEM_ENTRY_ID_MASK;
    item = item_id;
    status = formation_item_can_equip_in_slot(unit, slot, item_id);
    if (status < 0) {
        return status;
    }
    if (unit != FORMATION_PREVIEW_UNIT_INDEX) {
        item_adjust_inventory_count(g_formation_unit_pointers[unit]->equipment[slot], 1);
    }
    if (slot < 2 && status != 1) {
        other = slot == 0;
        if (unit != FORMATION_PREVIEW_UNIT_INDEX) {
            item_adjust_inventory_count(g_formation_unit_pointers[unit]->equipment[other], 1);
        }
        g_formation_unit_pointers[unit]->equipment[other] = ITEM_ID_NOTHING;
    }
    g_formation_unit_pointers[unit]->equipment[slot] = item;
    if (unit != FORMATION_PREVIEW_UNIT_INDEX) {
        formation_save_records();
    }
    return 1;
}

/* Whether a unit's job can equip an item: 1 for no item or an item whose
 * type bit is set in the unit's equippable-types bitfield, -1 for an item
 * of the "other" menu category or a type the unit cannot use. The low ten
 * bits of the entry are the item id. */
s32 equipment_can_equip_item(s16 unit_index, s16 item_id) {
    s32 id = item_id & ITEM_ENTRY_ID_MASK;
    s32 result;

    if (id == ITEM_ID_NOTHING) {
        return 1;
    }
    if (item_get_menu_category(id) == ITEM_MENU_CATEGORY_OTHER) {
        return -1;
    }
    bits_init_primary_reader(g_formation_unit_pointers[unit_index]->equippable_item_types);
    bits_read_primary(item_get_type(id));
    if (bits_read_primary(1) == 0) {
        result = -1;
    } else {
        result = 1;
    }
    return result;
}

/*
 * Checks whether an item may go into a formation unit's equipment slot.
 *
 * Returns a negative lock result for a locked unit, -1 when the unit cannot
 * use the item, 0 when it fits only with the other hand emptied, and 1 when
 * it fits. Hand slots (0 and 1) consider the item in the opposite hand: two
 * weapons need both to be Two Swords compatible and the unit to have Two
 * Swords; a shield cannot pair with another shield or a forced-two-hands
 * weapon.
 *
 * The s16 id and its new_item copy reproduce the target's two callee-saved
 * copies of the masked id; an s32 id folds them into one register.
 */
s32 formation_item_can_equip_in_slot(s16 unit_index, s16 slot, s16 item_id) {
    s32 result = formation_get_unit_lock(unit_index);
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

    if (result < 0) {
        return result;
    }
    id = item_id & ITEM_ENTRY_ID_MASK;
    new_item = id;
    if (id == ITEM_ID_NOTHING) {
        return 1;
    }
    result = equipment_can_equip_item(unit_index, id);
    if (slot < MENU_EQUIPMENT_SLOT_HEAD) {
        if (result != 1) {
            return result;
        }
        is_nonstandard_weapon = id >= ITEM_ID_THROWABLE_FIRST;
        forced_two_hands = 0;
        if (is_nonstandard_weapon) {
            two_swords = 0;
        } else {
            flags = g_item_weapon_data[id].flags;
            forced_two_hands = flags & WEAPON_FLAG_FORCED_TWO_HANDS;
            two_swords = flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE;
        }
        other_slot = slot == MENU_EQUIPMENT_SLOT_RIGHT_HAND;
        other = g_formation_unit_pointers[unit_index]->equipment[other_slot] & ITEM_ENTRY_ID_MASK;
        other_is_nonstandard_weapon = other >= ITEM_ID_THROWABLE_FIRST;
        other_forced_two_hands = 0;
        if (other_is_nonstandard_weapon) {
            other_two_swords = 0;
        } else {
            flags = g_item_weapon_data[other].flags;
            other_forced_two_hands = flags & WEAPON_FLAG_FORCED_TWO_HANDS;
            other_two_swords = flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE;
        }

        if (two_swords && other_two_swords && ability_has_two_swords(unit_index)) {
            return 1;
        }
        if (!is_nonstandard_weapon && !other_is_nonstandard_weapon && new_item != ITEM_ID_NOTHING
            && other != ITEM_ID_NOTHING) {
            return 0;
        }
        if (is_nonstandard_weapon && other_is_nonstandard_weapon) {
            return 0;
        }
        if (is_nonstandard_weapon && other_forced_two_hands) {
            return 0;
        }
        if (other_is_nonstandard_weapon && forced_two_hands) {
            return 0;
        }
    }
    return result;
}

/*
 * Change the unit's job and optionally select replacement equipment.
 *
 * Mime removes all equipment. Whatever the new job cannot use is dropped
 * (formation_remove_invalid_loadout). With `update_equipment`, a
 * roster unit that is not a Mime gets its best-fit equipment and the
 * auto-equip banner. The preview record does not exchange inventory items;
 * ordinary slots return their old equipment before reserving replacements.
 */
s32 formation_job_change(s16 unit_id, s16 job_id, s32 update_equipment) {
    s16 equipment[MENU_EQUIPMENT_SLOT_COUNT];
    s32 i;

    g_formation_unit_pointers[unit_id]->identity.job_id = job_id;
    if (job_id == JOB_ID_MIME) {
        for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
            if (unit_id != FORMATION_PREVIEW_UNIT_INDEX) {
                item_adjust_inventory_count(g_formation_unit_pointers[unit_id]->equipment[i], 1);
            }
            g_formation_unit_pointers[unit_id]->equipment[i] = ITEM_ID_NOTHING;
        }
    }
    formation_save_records();
    formation_remove_invalid_loadout(unit_id);
    if (update_equipment != 0) {
        if (unit_id != FORMATION_PREVIEW_UNIT_INDEX) {
            if (g_formation_unit_pointers[unit_id]->identity.job_id != JOB_ID_MIME) {
                formation_item_build_best_fit(unit_id, equipment);
                for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                    item_adjust_inventory_count(g_formation_unit_pointers[unit_id]->equipment[i], 1);
                }
                for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                    g_formation_unit_pointers[unit_id]->equipment[i] = equipment[i];
                    item_adjust_inventory_count(equipment[i], -1);
                }
                formation_save_records();
                formation_banner_countdown(0xD811, 0x1E);
            }
        }
    }
    return 1;
}

/*
 * Drop equipment and abilities a formation unit can no longer use.
 *
 * Items formation_item_can_equip_in_slot refuses (0, -1 or the Mime
 * lock) and a second hand weapon without Two Swords are removed, returning
 * items to inventory except for the preview record.
 * Mime clears every ability slot, jobs with an innate support ability drop
 * the redundant copy, and a secondary skillset equal to the primary is
 * cleared. The loop counter and status locals are reused for the two hand
 * items, which keeps the original register assignment.
 */
void formation_remove_invalid_loadout(s16 unit_id) {
    s32 i;
    s32 status;

    for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
        status = formation_item_can_equip_in_slot(unit_id, i, g_formation_unit_pointers[unit_id]->equipment[i]);
        if (status == -1 || status == 0 || status == FORMATION_UNIT_LOCK_MIME) {
            if (unit_id != FORMATION_PREVIEW_UNIT_INDEX) {
                item_adjust_inventory_count(g_formation_unit_pointers[unit_id]->equipment[i], 1);
            }
            g_formation_unit_pointers[unit_id]->equipment[i] = ITEM_ID_NOTHING;
        }
    }
    i = g_formation_unit_pointers[unit_id]->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND];
    status = g_formation_unit_pointers[unit_id]->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND];
    if (i < ITEM_ID_THROWABLE_FIRST && i != ITEM_ID_NOTHING && status < ITEM_ID_THROWABLE_FIRST
        && status != ITEM_ID_NOTHING && ability_has_two_swords(unit_id) == 0) {
        if (unit_id != FORMATION_PREVIEW_UNIT_INDEX) {
            item_adjust_inventory_count(
                g_formation_unit_pointers[unit_id]->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND], 1);
        }
        g_formation_unit_pointers[unit_id]->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_ID_NOTHING;
    }
    if (g_formation_unit_pointers[unit_id]->identity.job_id == JOB_ID_MIME) {
        memset(g_formation_unit_pointers[unit_id]->abilities, 0, sizeof(g_formation_unit_pointers[unit_id]->abilities));
    }
    if (g_formation_unit_pointers[unit_id]->identity.job_id == JOB_ID_CHEMIST
        && g_formation_unit_pointers[unit_id]->abilities[FORMATION_ABILITY_SLOT_SUPPORT]
            == ABILITY_ID_SUPPORT_THROW_ITEM) {
        g_formation_unit_pointers[unit_id]->abilities[FORMATION_ABILITY_SLOT_SUPPORT] = 0;
    }
    if (g_formation_unit_pointers[unit_id]->identity.job_id == JOB_ID_MONK
        && g_formation_unit_pointers[unit_id]->abilities[FORMATION_ABILITY_SLOT_SUPPORT]
            == ABILITY_ID_SUPPORT_MARTIAL_ARTS) {
        g_formation_unit_pointers[unit_id]->abilities[FORMATION_ABILITY_SLOT_SUPPORT] = 0;
    }
    if (g_formation_unit_pointers[unit_id]->identity.job_id == JOB_ID_MEDIATOR
        && g_formation_unit_pointers[unit_id]->abilities[FORMATION_ABILITY_SLOT_SUPPORT]
            == ABILITY_ID_SUPPORT_MONSTER_TALK) {
        g_formation_unit_pointers[unit_id]->abilities[FORMATION_ABILITY_SLOT_SUPPORT] = 0;
    }
    if (g_formation_unit_pointers[unit_id]->identity.job_id == JOB_ID_NINJA
        && g_formation_unit_pointers[unit_id]->abilities[FORMATION_ABILITY_SLOT_SUPPORT]
            == ABILITY_ID_SUPPORT_TWO_SWORDS) {
        g_formation_unit_pointers[unit_id]->abilities[FORMATION_ABILITY_SLOT_SUPPORT] = 0;
    }
    if (g_formation_unit_pointers[unit_id]->abilities[FORMATION_ABILITY_SLOT_PRIMARY_SKILLSET]
        == g_formation_unit_pointers[unit_id]->abilities[FORMATION_ABILITY_SLOT_SECONDARY_SKILLSET]) {
        g_formation_unit_pointers[unit_id]->abilities[FORMATION_ABILITY_SLOT_SECONDARY_SKILLSET] = 0;
    }
    if (unit_id != FORMATION_PREVIEW_UNIT_INDEX) {
        formation_save_records();
    }
}

/*
 * Build the -1-terminated item list for an item menu category and sort it.
 *
 * Items 1-253 are listed when held (mode 3 counts inventory only, others
 * include equipped copies) and their type fits the category. Mode 1 flags
 * items the unit cannot equip with 0x4000, or hides them like mode 2 when Show
 * Unequippable is not On. Categories 0-4 and 7 with sort modes 1-5 keep the
 * tab's stored order (item_sort_list_by_category_order, which returns the new
 * count); otherwise (another category, or a sort mode such as one with bit
 * 0x80 set) the list is sorted by item_sort_id_list key list
 * `sort_mode & 0xF`.
 */
s32 formation_item_build_list(s16 unit_id, s16 sort_mode, s8 category, s16* entries, u8 mode) {
    s32 count;
    s32 item;
    s32 entry;
    s32 type;
    s32 result;
    s32 list_index;

    if (mode == ITEM_LIST_MODE_MARK_UNEQUIPPABLE && g_game_options.fields.show_unequippable_items != GAME_OPTION_ON) {
        mode = ITEM_LIST_MODE_EQUIPPABLE;
    }
    item = 1;
    count = 0;
    for (; item < ITEM_ID_END; item++) {
        entry = item;
        if (mode == ITEM_LIST_MODE_EQUIPPABLE_IN_INVENTORY) {
            result = item_adjust_inventory_count(item, 0);
        } else {
            result = item_get_total_count(item);
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
            result = equipment_can_equip_item(unit_id, entry);
            if (mode >= 2 && result == -1) {
                continue;
            }
            if (mode == ITEM_LIST_MODE_MARK_UNEQUIPPABLE && result == -1) {
                entry |= ITEM_ENTRY_UNEQUIPPABLE;
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
        item_sort_id_list(sort_mode & 0xF, entries);
    }
    return count;
}

/* Select the equipment slot for an item.
 *
 * Weapons and shields choose a hand based on what the unit already holds.
 */
menu_equipment_slot_e formation_item_select_slot(s16 unit_index, s32 item_id) {
    formation_unit_t* unit = g_formation_unit_pointers[unit_index];
    s32 right_hand;
    s32 left_hand;
    menu_equipment_slot_e slot;

    item_id &= ITEM_ENTRY_ID_MASK;
    right_hand = unit->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND];
    left_hand = unit->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND];
    if (item_id < ITEM_ID_THROWABLE_FIRST) {
        if (left_hand < ITEM_ID_THROWABLE_FIRST) {
            if (right_hand >= ITEM_ID_THROWABLE_FIRST) {
                slot = MENU_EQUIPMENT_SLOT_LEFT_HAND;
            } else {
                slot = left_hand != ITEM_ID_NOTHING;
            }
        } else {
            slot = MENU_EQUIPMENT_SLOT_RIGHT_HAND;
        }
    } else if (item_id < ITEM_ID_HEADGEAR_FIRST) {
        if (left_hand < ITEM_ID_THROWABLE_FIRST && left_hand != ITEM_ID_NOTHING) {
            slot = MENU_EQUIPMENT_SLOT_RIGHT_HAND;
        } else if (right_hand < ITEM_ID_THROWABLE_FIRST && right_hand != ITEM_ID_NOTHING) {
            slot = MENU_EQUIPMENT_SLOT_LEFT_HAND;
        } else {
            slot = left_hand >= ITEM_ID_THROWABLE_FIRST;
        }
    } else if (item_id < ITEM_ID_BODY_ARMOR_FIRST) {
        slot = MENU_EQUIPMENT_SLOT_HEAD;
    } else if (item_id < ITEM_ID_ACCESSORY_FIRST) {
        slot = MENU_EQUIPMENT_SLOT_BODY;
    } else {
        slot = MENU_EQUIPMENT_SLOT_ACCESSORY;
    }
    return slot;
}
