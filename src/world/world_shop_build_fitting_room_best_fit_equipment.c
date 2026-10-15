#include "fft/world.h"

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
