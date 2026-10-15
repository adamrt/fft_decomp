/* Shop quantities and equipment slots. Its start is a boundary the binary proves; its end is a boundary the binary
 * proves. */

#include "fft/world.h"
#include "psx/types.h"

/* Adjust a Fur Shop poached-item quantity, with the target's ten-bit index
 * mask and saturating byte range. */
s32 world_shop_adjust_poached_item_quantity(s16 index, s32 delta) {
    s32 slot = index & WORLD_ITEM_ID_MASK;
    s32 value = g_main_item_poached_quantities[slot] + delta;

    if (value < 0) {
        value = 0;
    }
    if (value >= 0x100) {
        value = 0xFF;
    }
    g_main_item_poached_quantities[slot] = value;
    return value;
}

/* Choose the best-ranked owned items for each equipment slot, keeping compatible
 * hand combinations and swapping hands when needed. Martial Arts clears both.
 */
void world_unit_build_best_fit_equipment(s16 unit_index, s16* equipment) {
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
    right_hand = g_world_unit_pointers[unit_index]->equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND];
    left_hand = g_world_unit_pointers[unit_index]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND];
    if (world_ability_has_two_swords(unit_index)) {
        two_swords = world_ability_has_two_hands(unit_index) == 0;
    }
    held_off_hand = ITEM_ID_NOTHING;
    held_weapon = ITEM_ID_NOTHING;
    if (two_swords) {
        held_weapon = right_hand;
        g_world_best_fit_slot_categories[WORLD_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_LIST_FILTER_WEAPON;
        held_off_hand = left_hand;
        if (held_weapon == ITEM_ID_NOTHING) {
            held_weapon = left_hand;
            held_off_hand = ITEM_ID_NOTHING;
        }
    } else {
        g_world_best_fit_slot_categories[WORLD_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_LIST_FILTER_SHIELD;
        if (right_hand >= ITEM_ID_THROWABLE_FIRST)
            held_off_hand = right_hand;
        else if (right_hand != ITEM_ID_NOTHING)
            held_weapon = right_hand;
        if (left_hand >= ITEM_ID_THROWABLE_FIRST)
            held_off_hand = left_hand;
        else if (left_hand != ITEM_ID_NOTHING)
            held_weapon = left_hand;
    }

    for (slot = 0; slot < WORLD_EQUIPMENT_SLOT_COUNT; slot++) {
        category = g_world_best_fit_slot_categories[slot];
        count = world_item_build_category_list(
            unit_index, 7, g_world_best_fit_slot_categories[slot], list, ITEM_LIST_MODE_EQUIPPABLE_IN_INVENTORY);
        length = count;
        if (category == ITEM_LIST_FILTER_WEAPON) {
            if (held_weapon < ITEM_ID_THROWABLE_FIRST)
                list[length++] = held_weapon;
            if (held_off_hand < ITEM_ID_THROWABLE_FIRST)
                list[length++] = held_off_hand;
            list[length] = -1;
            item_sort_id_list_variant(7, list);
            if (world_ability_has_two_hands(unit_index)) {
                for (i = 0; list[i] != -1; i++) {
                    s16 item_id = list[i];
                    if (g_main_item_weapon_data[item_id].flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE) {
                        list[0] = item_id;
                        break;
                    }
                }
            } else if (two_swords) {
                retry = 0;
                for (i = 0;; i++) {
                    if (list[i] == -1) {
                        g_world_best_fit_slot_categories[WORLD_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_LIST_FILTER_SHIELD;
                        if (slot == WORLD_EQUIPMENT_SLOT_LEFT_HAND) {
                            retry = 1;
                            slot--;
                        }
                        break;
                    }
                    if (g_main_item_weapon_data[list[i]].flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE) {
                        if (slot == WORLD_EQUIPMENT_SLOT_RIGHT_HAND) {
                            list[0] = list[i];
                            break;
                        }
                        owned = item_adjust_inventory_count(list[i], 0);
                        if (held_weapon == list[i])
                            owned++;
                        if (held_off_hand == list[i])
                            owned++;
                        if (owned >= 2 || equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND] != list[i]) {
                            list[0] = list[i];
                            break;
                        }
                    }
                }
                if (retry)
                    continue;
            }
        } else if (category == ITEM_LIST_FILTER_SHIELD) {
            if ((world_ability_has_two_hands(unit_index)
                    && (g_main_item_weapon_data[equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND]].flags
                        & WEAPON_FLAG_TWO_HANDS_COMPATIBLE))
                || (g_main_item_weapon_data[equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND]].flags
                    & WEAPON_FLAG_FORCED_TWO_HANDS)) {
                list[0] = -1;
            } else {
                if (held_off_hand >= ITEM_ID_THROWABLE_FIRST)
                    list[length++] = held_off_hand;
                list[length] = -1;
                item_sort_id_list_variant(7, list);
            }
        } else {
            list[length++] = g_world_unit_pointers[unit_index]->equipment[slot];
            list[length] = -1;
            item_sort_id_list_variant(7, list);
        }
        if (list[0] <= ITEM_ID_NOTHING)
            equipment[slot] = ITEM_ID_NOTHING;
        else
            equipment[slot] = list[0];
    }

    i = 0; /* reused as the swap flag (the target keeps it in i's register) */
    if (g_world_unit_pointers[unit_index]->support_abilities[2] & BATTLE_SUPPORT_SET_3_MARTIAL_ARTS) {
        equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND] = ITEM_ID_NOTHING;
        equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_ID_NOTHING;
    }
    if (right_hand != ITEM_ID_NOTHING && right_hand == equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND]) {
        i = 1;
    } else if (left_hand != ITEM_ID_NOTHING && left_hand == equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND]) {
        i = 1;
    } else if (!two_swords && left_hand < ITEM_ID_THROWABLE_FIRST && left_hand != ITEM_ID_NOTHING) {
        i = 1;
    } else if (right_hand >= ITEM_ID_THROWABLE_FIRST) {
        i = 1;
    }
    if (i) {
        s32 swap = equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND];
        equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND] = equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND];
        equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND] = swap;
    }
}

/* Choose fitting-room equipment through the body slot, then mark newly reserved
 * shop items. The accessory slot is left to the caller.
 */
void world_shop_build_fitting_room_best_fit_equipment(s16 unit_index, s16 shop_id, s16* out) {
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

    reserved = g_world_shop_fitting_room_items[unit_index][WORLD_EQUIPMENT_SLOT_RIGHT_HAND];
    if (reserved != ITEM_ID_NOTHING) {
        right = reserved;
    } else {
        right = g_world_unit_pointers[unit_index]->equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND] & WORLD_ITEM_ID_MASK;
    }
    reserved = g_world_shop_fitting_room_items[unit_index][WORLD_EQUIPMENT_SLOT_LEFT_HAND];
    if (reserved != ITEM_ID_NOTHING) {
        left = reserved;
    } else {
        left = g_world_unit_pointers[unit_index]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND] & WORLD_ITEM_ID_MASK;
    }

    second = ITEM_ID_NOTHING;
    first = ITEM_ID_NOTHING;
    dual = 0;
    if (world_ability_has_two_swords(unit_index)) {
        dual = world_ability_has_two_hands(unit_index) == 0;
    }
    if (dual) {
        second = left;
        g_world_fitting_room_best_fit_slot_categories[WORLD_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_LIST_FILTER_WEAPON;
        /* Written last so it is the instruction immediately before the test:
         * reorg scans backward from the branch and takes it into the delay
         * slot, which is where the target has `sw s8,328(sp)`. */
        first = right;
        if (first == ITEM_ID_NOTHING) {
            first = left;
            second = ITEM_ID_NOTHING;
        }
    } else {
        g_world_fitting_room_best_fit_slot_categories[WORLD_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_LIST_FILTER_SHIELD;
        if (right >= ITEM_ID_THROWABLE_FIRST)
            second = right;
        else if (right != ITEM_ID_NOTHING)
            first = right;
        if (left >= ITEM_ID_THROWABLE_FIRST)
            second = left;
        else if (left != ITEM_ID_NOTHING)
            first = left;
    }

    for (slot = 0; slot < WORLD_EQUIPMENT_SLOT_ACCESSORY; slot++) {
        category = g_world_fitting_room_best_fit_slot_categories[slot];
        count = world_shop_build_item_list(unit_index, shop_id, category, list, ITEM_LIST_MODE_EQUIPPABLE);
        length = count;
        if (category == ITEM_LIST_FILTER_WEAPON) {
            /* One lvalue per arm: the target computes &list[length] once and stores
             * through it from both sides of the test (`sh t0,0(v1)` then
             * `sh zero,0(a0)`), with `length` post-incremented. Two separate
             * `list[length] = ...` statements duplicate the address computation. */
            if (slot == WORLD_EQUIPMENT_SLOT_RIGHT_HAND) {
                list[length++] = (first < ITEM_ID_THROWABLE_FIRST) ? first : ITEM_ID_NOTHING;
            }
            if (slot == WORLD_EQUIPMENT_SLOT_LEFT_HAND) {
                list[length++] = (second < ITEM_ID_THROWABLE_FIRST) ? second : ITEM_ID_NOTHING;
            }
            list[length] = -1;
            item_sort_id_list_variant(7, list);
            if (world_ability_has_two_hands(unit_index)) {
                for (i = 0; list[i] != -1; i++) {
                    s16 item_id = list[i];
                    if (g_main_item_weapon_data[item_id].flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE) {
                        list[0] = item_id;
                        break;
                    }
                }
            } else if (dual) {
                retry = 0;
                for (i = 0;; i++) {
                    if (list[i] == -1) {
                        g_world_fitting_room_best_fit_slot_categories[WORLD_EQUIPMENT_SLOT_LEFT_HAND]
                            = ITEM_LIST_FILTER_SHIELD;
                        if (slot == WORLD_EQUIPMENT_SLOT_LEFT_HAND) {
                            retry = 1;
                            slot--;
                        }
                        break;
                    }
                    if (g_main_item_weapon_data[list[i]].flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE) {
                        list[0] = list[i];
                        break;
                    }
                }
                if (retry)
                    continue;
            }
        } else if (category == ITEM_LIST_FILTER_SHIELD) {
            if ((world_ability_has_two_hands(unit_index)
                    && (g_main_item_weapon_data[out[WORLD_EQUIPMENT_SLOT_RIGHT_HAND]].flags
                        & WEAPON_FLAG_TWO_HANDS_COMPATIBLE))
                || (g_main_item_weapon_data[out[WORLD_EQUIPMENT_SLOT_RIGHT_HAND]].flags
                    & WEAPON_FLAG_FORCED_TWO_HANDS)) {
                list[0] = -1;
            } else {
                if (second >= ITEM_ID_THROWABLE_FIRST)
                    list[length++] = second;
                list[length] = -1;
                item_sort_id_list_variant(7, list);
            }
        } else {
            list[length++] = g_world_unit_pointers[unit_index]->equipment[slot] & WORLD_ITEM_ID_MASK;
            list[length] = -1;
            item_sort_id_list_variant(7, list);
        }
        if (list[0] <= ITEM_ID_NOTHING)
            out[slot] = ITEM_ID_NOTHING;
        else
            out[slot] = list[0];
    }

    i = 0; /* reused as the swap flag, as in the sibling */
    if (g_world_unit_pointers[unit_index]->support_abilities[2] & BATTLE_SUPPORT_SET_3_MARTIAL_ARTS) {
        out[WORLD_EQUIPMENT_SLOT_RIGHT_HAND] = ITEM_ID_NOTHING;
        out[WORLD_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_ID_NOTHING;
    }
    if ((right != ITEM_ID_NOTHING && right == out[WORLD_EQUIPMENT_SLOT_LEFT_HAND])
        || (left != ITEM_ID_NOTHING && left == out[WORLD_EQUIPMENT_SLOT_RIGHT_HAND])
        || (!dual && left < ITEM_ID_THROWABLE_FIRST && left != ITEM_ID_NOTHING) || right >= ITEM_ID_THROWABLE_FIRST)
        i = 1;
    if (i) {
        /* The target keeps the swap flag and the swap temporary in ONE
         * register (a0), and gcc 2.6.3 never splits live ranges, so the two
         * non-overlapping roles were one variable in the original source. */
        i = out[WORLD_EQUIPMENT_SLOT_RIGHT_HAND];
        out[WORLD_EQUIPMENT_SLOT_RIGHT_HAND] = out[WORLD_EQUIPMENT_SLOT_LEFT_HAND];
        out[WORLD_EQUIPMENT_SLOT_LEFT_HAND] = i;
    }

    for (slot = 0; slot < WORLD_EQUIPMENT_SLOT_ACCESSORY; slot++) {
        s16 item = out[slot];
        if (item != g_world_unit_pointers[unit_index]->equipment[slot]
            && item != g_world_shop_fitting_room_items[unit_index][slot]) {
            out[slot] = item | WORLD_EQUIPMENT_SHOP_ITEM;
        }
    }
}

/* Install an item into a formation unit's equipment slot.
 *
 * The preview record is updated without touching inventory or derived stats.
 * Equipping a hand slot while two-handing also returns the other hand's item
 * and clears that slot.
 *
 * `item` is a separate u16 mirror of the masked argument: one s32 variable
 * reused for both the call argument and the stored value is copy-propagated and
 * swaps which of the two ends up in the callee-saved register.
 */
s32 world_formation_equip_item_to_unit_slot(s16 unit, s16 slot, s16 item_id) {
    s32 status;
    s32 other;
    u16 item;

    item_id &= WORLD_ITEM_ID_MASK;
    item = item_id;
    status = world_formation_can_equip_item_in_slot(unit, slot, item_id);
    if (status < 0) {
        return status;
    }
    if (unit != WORLD_FORMATION_PREVIEW_UNIT_INDEX) {
        item_adjust_inventory_count(g_world_unit_pointers[unit]->equipment[slot], 1);
    }
    if (slot < 2 && status != 1) {
        other = slot == 0;
        if (unit != WORLD_FORMATION_PREVIEW_UNIT_INDEX) {
            item_adjust_inventory_count(g_world_unit_pointers[unit]->equipment[other], 1);
        }
        g_world_unit_pointers[unit]->equipment[other] = ITEM_ID_NOTHING;
    }
    g_world_unit_pointers[unit]->equipment[slot] = item;
    if (unit != WORLD_FORMATION_PREVIEW_UNIT_INDEX) {
        world_formation_save_records_to_party();
    }
    return 1;
}

/* Whether a unit's job can equip an item: 1 for no item or an item whose
 * type bit is set in the unit's equippable-types bitfield, -1 for an item
 * of the "other" menu category or a type the unit cannot use. The low ten
 * bits of the entry are the item id. */
s32 world_unit_can_equip_item_id(s16 unit_index, s16 item_id) {
    s32 id = item_id & WORLD_ITEM_ID_MASK;
    s32 result;

    if (id == ITEM_ID_NOTHING) {
        return 1;
    }
    if (item_get_menu_category(id) == ITEM_MENU_CATEGORY_OTHER) {
        return -1;
    }
    bits_init_primary_reader_variant(g_world_unit_pointers[unit_index]->equippable_item_types);
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
 * Hand slots (0 and 1) also consider the item in the opposite hand: two
 * weapons need both to be Two Swords compatible and the unit to have Two
 * Swords; a shield cannot pair with another shield or a forced-two-hands
 * weapon.
 *
 * The s16 id and its new_item copy reproduce the target's two callee-saved
 * copies of the masked id; an s32 id folds them into one register.
 */
s32 world_formation_can_equip_item_in_slot(s16 unit_index, s16 slot, s16 item_id) {
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

    result = world_formation_get_unit_lock(unit_index);
    if (result < 0) {
        return result;
    }
    id = item_id & WORLD_ITEM_ID_MASK;
    new_item = id;
    if (id == ITEM_ID_NOTHING) {
        return 1;
    }
    result = world_unit_can_equip_item_id(unit_index, id);
    if (slot < WORLD_EQUIPMENT_SLOT_HEAD) {
        if (result != 1) {
            return result;
        }
        is_nonstandard_weapon = id >= ITEM_ID_THROWABLE_FIRST;
        forced_two_hands = 0;
        if (is_nonstandard_weapon) {
            two_swords = 0;
        } else {
            flags = g_main_item_weapon_data[id].flags;
            forced_two_hands = flags & WEAPON_FLAG_FORCED_TWO_HANDS;
            two_swords = flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE;
        }
        other_slot = slot == WORLD_EQUIPMENT_SLOT_RIGHT_HAND;
        other = g_world_unit_pointers[unit_index]->equipment[other_slot] & WORLD_ITEM_ID_MASK;
        other_is_nonstandard_weapon = other >= ITEM_ID_THROWABLE_FIRST;
        other_forced_two_hands = 0;
        if (other_is_nonstandard_weapon) {
            other_two_swords = 0;
        } else {
            flags = g_main_item_weapon_data[other].flags;
            other_forced_two_hands = flags & WEAPON_FLAG_FORCED_TWO_HANDS;
            other_two_swords = flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE;
        }

        if (two_swords && other_two_swords && world_ability_has_two_swords(unit_index)) {
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
 * Mime removes all equipment. The preview record does not exchange inventory
 * items; ordinary slots return their old equipment before reserving replacements.
 */
s32 world_formation_change_unit_job(s16 unit_id, s16 job_id, s32 update_equipment) {
    s16 equipment[WORLD_EQUIPMENT_SLOT_COUNT];
    s32 i;
    g_world_unit_pointers[unit_id]->identity.job_id = job_id;
    if (job_id == JOB_ID_MIME) {
        for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
            if (unit_id != WORLD_FORMATION_PREVIEW_UNIT_INDEX) {
                item_adjust_inventory_count(g_world_unit_pointers[unit_id]->equipment[i], 1);
            }
            g_world_unit_pointers[unit_id]->equipment[i] = ITEM_ID_NOTHING;
        }
    }
    world_formation_save_records_to_party();
    world_formation_remove_invalid_unit_loadout(unit_id);
    if (update_equipment != 0) {
        if (unit_id != WORLD_FORMATION_PREVIEW_UNIT_INDEX) {
            if (g_world_unit_pointers[unit_id]->identity.job_id != JOB_ID_MIME) {
                world_unit_build_best_fit_equipment(unit_id, equipment);
                for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
                    item_adjust_inventory_count(g_world_unit_pointers[unit_id]->equipment[i], 1);
                }
                for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
                    g_world_unit_pointers[unit_id]->equipment[i] = equipment[i];
                    item_adjust_inventory_count(equipment[i], -1);
                }
                world_formation_save_records_to_party();
                world_menu_run_unit_status_banner_countdown(0xD811, 0x1E);
            }
        }
    }
    return 1;
}

/*
 * Drop equipment and abilities a formation unit can no longer use.
 *
 * Unequippable items (status 0, -1 or -6) and a second hand weapon without
 * Two Swords are removed, returning items to inventory except for the preview
 * record.
 * Mime clears every ability slot, jobs with an innate support ability drop
 * the redundant copy, and a secondary skillset equal to the primary is
 * cleared. The loop counter and status locals are reused for the two hand
 * items, which keeps the original register assignment.
 */
void world_formation_remove_invalid_unit_loadout(s16 unit_id) {
    s32 i;
    s32 status;

    for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
        status = world_formation_can_equip_item_in_slot(unit_id, i, g_world_unit_pointers[unit_id]->equipment[i]);
        if (status == -1 || status == 0 || status == -6) {
            if (unit_id != WORLD_FORMATION_PREVIEW_UNIT_INDEX) {
                item_adjust_inventory_count(g_world_unit_pointers[unit_id]->equipment[i], 1);
            }
            g_world_unit_pointers[unit_id]->equipment[i] = ITEM_ID_NOTHING;
        }
    }
    i = g_world_unit_pointers[unit_id]->equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND];
    status = g_world_unit_pointers[unit_id]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND];
    if (i < ITEM_ID_THROWABLE_FIRST && i != ITEM_ID_NOTHING && status < ITEM_ID_THROWABLE_FIRST
        && status != ITEM_ID_NOTHING && world_ability_has_two_swords(unit_id) == 0) {
        if (unit_id != WORLD_FORMATION_PREVIEW_UNIT_INDEX) {
            item_adjust_inventory_count(g_world_unit_pointers[unit_id]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND], 1);
        }
        g_world_unit_pointers[unit_id]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_ID_NOTHING;
    }
    if (g_world_unit_pointers[unit_id]->identity.job_id == JOB_ID_MIME) {
        memset(&g_world_unit_pointers[unit_id]->abilities, 0, sizeof(world_formation_abilities_t));
    }
    if (g_world_unit_pointers[unit_id]->identity.job_id == JOB_ID_CHEMIST
        && g_world_unit_pointers[unit_id]->abilities.fields.support_ability == ABILITY_ID_SUPPORT_THROW_ITEM) {
        g_world_unit_pointers[unit_id]->abilities.fields.support_ability = 0;
    }
    if (g_world_unit_pointers[unit_id]->identity.job_id == JOB_ID_MONK
        && g_world_unit_pointers[unit_id]->abilities.fields.support_ability == ABILITY_ID_SUPPORT_MARTIAL_ARTS) {
        g_world_unit_pointers[unit_id]->abilities.fields.support_ability = 0;
    }
    if (g_world_unit_pointers[unit_id]->identity.job_id == JOB_ID_MEDIATOR
        && g_world_unit_pointers[unit_id]->abilities.fields.support_ability == ABILITY_ID_SUPPORT_MONSTER_TALK) {
        g_world_unit_pointers[unit_id]->abilities.fields.support_ability = 0;
    }
    if (g_world_unit_pointers[unit_id]->identity.job_id == JOB_ID_NINJA
        && g_world_unit_pointers[unit_id]->abilities.fields.support_ability == ABILITY_ID_SUPPORT_TWO_SWORDS) {
        g_world_unit_pointers[unit_id]->abilities.fields.support_ability = 0;
    }
    if (g_world_unit_pointers[unit_id]->abilities.fields.primary_skillset
        == g_world_unit_pointers[unit_id]->abilities.fields.secondary_skillset) {
        g_world_unit_pointers[unit_id]->abilities.fields.secondary_skillset = 0;
    }
    if (unit_id != WORLD_FORMATION_PREVIEW_UNIT_INDEX) {
        world_formation_save_records_to_party();
    }
}

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
        item_sort_id_list_variant(sort_mode & 0xF, entries);
    }
    return count;
}

/* Select the equipment slot for an item.
 *
 * Weapons and shields choose a hand based on what the unit already holds.
 */
world_unit_equipment_slot_e world_item_select_equipment_slot(s16 unit_index, s32 item_id) {
    world_unit_t* unit = g_world_unit_pointers[unit_index];
    s32 right_hand;
    s32 left_hand;
    world_unit_equipment_slot_e slot;

    item_id &= WORLD_ITEM_ID_MASK;
    right_hand = unit->equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND];
    left_hand = unit->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND];
    if (item_id < ITEM_ID_THROWABLE_FIRST) {
        if (left_hand < ITEM_ID_THROWABLE_FIRST) {
            if (right_hand >= ITEM_ID_THROWABLE_FIRST) {
                slot = WORLD_EQUIPMENT_SLOT_LEFT_HAND;
            } else {
                slot = left_hand != ITEM_ID_NOTHING;
            }
        } else {
            slot = WORLD_EQUIPMENT_SLOT_RIGHT_HAND;
        }
    } else if (item_id < ITEM_ID_HEADGEAR_FIRST) {
        if (left_hand < ITEM_ID_THROWABLE_FIRST && left_hand != ITEM_ID_NOTHING) {
            slot = WORLD_EQUIPMENT_SLOT_RIGHT_HAND;
        } else if (right_hand < ITEM_ID_THROWABLE_FIRST && right_hand != ITEM_ID_NOTHING) {
            slot = WORLD_EQUIPMENT_SLOT_LEFT_HAND;
        } else {
            slot = left_hand >= ITEM_ID_THROWABLE_FIRST;
        }
    } else if (item_id < ITEM_ID_BODY_ARMOR_FIRST) {
        slot = WORLD_EQUIPMENT_SLOT_HEAD;
    } else if (item_id < ITEM_ID_ACCESSORY_FIRST) {
        slot = WORLD_EQUIPMENT_SLOT_BODY;
    } else {
        slot = WORLD_EQUIPMENT_SLOT_ACCESSORY;
    }
    return slot;
}
