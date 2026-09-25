/* The shop "fitting room" twin of world_unit_build_best_fit_equipment, which
 * ends exactly where this function starts and shares its whole shape. The
 * differences are all read off the target: the candidate lists come from the
 * shop builder world_shop_build_item_list (mode 2) rather than world_item_build_category_list,
 * the slot loop runs FOUR slots rather than five, each hand's current item is
 * the unit's fitting-room reservation when it has one, and every chosen item
 * that is neither already equipped nor already reserved is flagged 0x8000.
 *
 * 0x8018D7F4 is a SECOND five-byte copy of the slot-category table: the image
 * holds `05 06 02 03 04` at 0x8018D7EC (catalogued as g_world_best_fit_slot_categories[5], used by
 * world_unit_build_best_fit_equipment) and the same five bytes again at
 * 0x8018D7F4, three pad bytes later. It is a separate object, not an interior
 * offset of g_world_best_fit_slot_categories, and this function writes its entry 1 exactly as the
 * sibling writes g_world_best_fit_slot_categories[1].
 *
 * The range holds one function: its only `jr ra` is the terminator at
 * 0x80124420, and world_formation_equip_item_to_unit_slot follows at
 * 0x80124428.
 *
 * Load-bearing shapes:
 *   - The two hand lookups are if/else with a shared loaded temporary
 *     (`reserved`), not `x = a; if (x == 0) x = b;`, and each slot arm stores
 *     through one lvalue `list[length++]` with the value selected by the test.
 *   - Declaration order: the target spills `first` to sp+328 and `dual` to
 *     sp+336.
 *   - The swap flag and the swap temporary are one variable (both in a0).
 *   - `first = right` is written last in the dual arm, so reorg's backward
 *     scan takes it into the `bnez` delay slot.
 *
 * The 432-byte frame (args 24 + vars 368 + 10 saves) is what `s16 list[0x90]`
 * reproduces: sp+24 to sp+312 is exactly 0x120 bytes, and every slot above it
 * is a reload slot on the 8-byte stride.
 */
#include "fft/world.h"

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

    reserved = g_world_shop_fitting_room_items[unit_index][0];
    if (reserved != ITEM_ID_NOTHING) {
        right = reserved;
    } else {
        right = g_world_formation_unit_pointers[unit_index]->equipment[0] & WORLD_ITEM_ID_MASK;
    }
    reserved = g_world_shop_fitting_room_items[unit_index][1];
    if (reserved != ITEM_ID_NOTHING) {
        left = reserved;
    } else {
        left = g_world_formation_unit_pointers[unit_index]->equipment[1] & WORLD_ITEM_ID_MASK;
    }

    second = ITEM_ID_NOTHING;
    first = ITEM_ID_NOTHING;
    dual = 0;
    if (world_ability_has_two_swords(unit_index)) {
        dual = world_ability_has_two_hands(unit_index) == 0;
    }
    if (dual) {
        second = left;
        g_world_fitting_room_best_fit_slot_categories[1] = 5;
        /* Written last so it is the instruction immediately before the test:
         * reorg scans backward from the branch and takes it into the delay
         * slot, which is where the target has `sw s8,328(sp)`. */
        first = right;
        if (first == ITEM_ID_NOTHING) {
            first = left;
            second = ITEM_ID_NOTHING;
        }
    } else {
        g_world_fitting_room_best_fit_slot_categories[1] = 6;
        if (right >= ITEM_ID_THROWABLE_FIRST)
            second = right;
        else if (right != ITEM_ID_NOTHING)
            first = right;
        if (left >= ITEM_ID_THROWABLE_FIRST)
            second = left;
        else if (left != ITEM_ID_NOTHING)
            first = left;
    }

    for (slot = 0; slot < 4; slot++) {
        category = g_world_fitting_room_best_fit_slot_categories[slot];
        count = world_shop_build_item_list(unit_index, shop_id, category, (world_item_list_entry_t*)list, 2);
        length = count;
        if (category == 5) {
            /* One lvalue per arm: the target computes &list[length] once and stores
             * through it from both sides of the test (`sh t0,0(v1)` then
             * `sh zero,0(a0)`), with `length` post-incremented. Two separate
             * `list[length] = ...` statements duplicate the address computation. */
            if (slot == 0) {
                list[length++] = (first < ITEM_ID_THROWABLE_FIRST) ? first : ITEM_ID_NOTHING;
            }
            if (slot == 1) {
                list[length++] = (second < ITEM_ID_THROWABLE_FIRST) ? second : ITEM_ID_NOTHING;
            }
            list[length] = -1;
            world_item_sort_id_list(7, (world_item_list_entry_t*)list);
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
                        g_world_fitting_room_best_fit_slot_categories[1] = 6;
                        if (slot == 1) {
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
        } else if (category == 6) {
            if ((world_ability_has_two_hands(unit_index)
                    && (g_main_item_weapon_data[out[0]].flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE))
                || (g_main_item_weapon_data[out[0]].flags & WEAPON_FLAG_FORCED_TWO_HANDS)) {
                list[0] = -1;
            } else {
                if (second >= ITEM_ID_THROWABLE_FIRST)
                    list[length++] = second;
                list[length] = -1;
                world_item_sort_id_list(7, (world_item_list_entry_t*)list);
            }
        } else {
            list[length++] = g_world_formation_unit_pointers[unit_index]->equipment[slot] & WORLD_ITEM_ID_MASK;
            list[length] = -1;
            world_item_sort_id_list(7, (world_item_list_entry_t*)list);
        }
        if (list[0] <= ITEM_ID_NOTHING)
            out[slot] = ITEM_ID_NOTHING;
        else
            out[slot] = list[0];
    }

    i = 0; /* reused as the swap flag, as in the sibling */
    if (g_world_formation_unit_pointers[unit_index]->support_sets_3 & BATTLE_SUPPORT_SET_3_MARTIAL_ARTS) {
        out[0] = ITEM_ID_NOTHING;
        out[1] = ITEM_ID_NOTHING;
    }
    if ((right != ITEM_ID_NOTHING && right == out[1]) || (left != ITEM_ID_NOTHING && left == out[0])
        || (!dual && left < ITEM_ID_THROWABLE_FIRST && left != ITEM_ID_NOTHING) || right >= ITEM_ID_THROWABLE_FIRST)
        i = 1;
    if (i) {
        /* The target keeps the swap flag and the swap temporary in ONE
         * register (a0), and gcc 2.6.3 never splits live ranges, so the two
         * non-overlapping roles were one variable in the original source. */
        i = out[0];
        out[0] = out[1];
        out[1] = i;
    }

    for (slot = 0; slot < 4; slot++) {
        s16 item = out[slot];
        if (item != g_world_formation_unit_pointers[unit_index]->equipment[slot]
            && item != g_world_shop_fitting_room_items[unit_index][slot]) {
            out[slot] = item | 0x8000;
        }
    }
}
