/* "Equip Best" (also known as "Optimize"): fills equipment[5] (right hand, left hand,
 * head, body, accessory) with the best-ranked held items for formation unit
 * `unit_index`; called by world_menu_run_best_fit_equipment. Each slot's list
 * comes from world_item_build_category_list plus the unit's current weapons,
 * sorted by world_item_sort_id_list (sort mode 7). Two-hands users prefer a compatible
 * weapon; two-swords users pick a second compatible weapon for the left
 * hand (owned count >= 2 or different from the right-hand pick), falling back
 * to the shield category. Throwable items and later IDs are never re-offered
 * as weapons. Martial Arts clears both hands so the unit remains unarmed. */
#include "fft/world.h"

/* Item-menu category per equipment slot (right hand, left hand, head, body,
 * accessory); slot 1 is 5 (weapon) for a two-swords user, else 6 (shield). */

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
    right_hand = g_world_formation_unit_pointers[unit_index]->equipment[0];
    left_hand = g_world_formation_unit_pointers[unit_index]->equipment[1];
    if (world_ability_has_two_swords(unit_index)) {
        two_swords = world_ability_has_two_hands(unit_index) == 0;
        /* Zero-instruction barrier: without it global allocation swaps the
         * callee-saved registers of `held_off_hand` and `equipment` (s5/s6). */
        __asm__ volatile("");
    }
    held_weapon = ITEM_ID_NOTHING;
    held_off_hand = ITEM_ID_NOTHING;
    if (two_swords) {
        held_weapon = right_hand;
        g_world_best_fit_slot_categories[1] = 5;
        held_off_hand = left_hand;
        if (held_weapon == ITEM_ID_NOTHING) {
            held_weapon = left_hand;
            held_off_hand = ITEM_ID_NOTHING;
        }
    } else {
        g_world_best_fit_slot_categories[1] = 6;
        if (right_hand >= ITEM_ID_THROWABLE_FIRST)
            held_off_hand = right_hand;
        else if (right_hand != ITEM_ID_NOTHING)
            held_weapon = right_hand;
        if (left_hand >= ITEM_ID_THROWABLE_FIRST)
            held_off_hand = left_hand;
        else if (left_hand != ITEM_ID_NOTHING)
            held_weapon = left_hand;
    }

    for (slot = 0; slot < 5; slot++) {
        category = g_world_best_fit_slot_categories[slot];
        count = world_item_build_category_list(
            unit_index, 7, g_world_best_fit_slot_categories[slot], (world_item_list_entry_t*)list, 3);
        length = count;
        if (category == 5) {
            if (held_weapon < ITEM_ID_THROWABLE_FIRST)
                list[length++] = held_weapon;
            if (held_off_hand < ITEM_ID_THROWABLE_FIRST)
                list[length++] = held_off_hand;
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
            } else if (two_swords) {
                retry = 0;
                for (i = 0;; i++) {
                    if (list[i] == -1) {
                        g_world_best_fit_slot_categories[1] = 6;
                        if (slot == 1) {
                            retry = 1;
                            slot--;
                        }
                        break;
                    }
                    if (g_main_item_weapon_data[list[i]].flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE) {
                        if (slot == 0) {
                            list[0] = list[i];
                            break;
                        }
                        owned = world_item_change_quantity_on_equip(list[i], 0);
                        if (held_weapon == list[i])
                            owned++;
                        if (held_off_hand == list[i])
                            owned++;
                        if (owned >= 2 || equipment[0] != list[i]) {
                            list[0] = list[i];
                            break;
                        }
                    }
                }
                if (retry)
                    continue;
            }
        } else if (category == 6) {
            if ((world_ability_has_two_hands(unit_index)
                    && (g_main_item_weapon_data[equipment[0]].flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE))
                || (g_main_item_weapon_data[equipment[0]].flags & WEAPON_FLAG_FORCED_TWO_HANDS)) {
                list[0] = -1;
            } else {
                if (held_off_hand >= ITEM_ID_THROWABLE_FIRST)
                    list[length++] = held_off_hand;
                list[length] = -1;
                world_item_sort_id_list(7, (world_item_list_entry_t*)list);
            }
        } else {
            list[length++] = g_world_formation_unit_pointers[unit_index]->equipment[slot];
            list[length] = -1;
            world_item_sort_id_list(7, (world_item_list_entry_t*)list);
        }
        if (list[0] <= ITEM_ID_NOTHING)
            equipment[slot] = ITEM_ID_NOTHING;
        else
            equipment[slot] = list[0];
    }

    i = 0; /* reused as the swap flag (the target keeps it in i's register) */
    if (g_world_formation_unit_pointers[unit_index]->support_abilities[2] & BATTLE_SUPPORT_SET_3_MARTIAL_ARTS) {
        equipment[0] = ITEM_ID_NOTHING;
        equipment[1] = ITEM_ID_NOTHING;
    }
    if ((right_hand != ITEM_ID_NOTHING && right_hand == equipment[1])
        || (left_hand != ITEM_ID_NOTHING && left_hand == equipment[0])
        || (!two_swords && left_hand < ITEM_ID_THROWABLE_FIRST && left_hand != ITEM_ID_NOTHING)
        || right_hand >= ITEM_ID_THROWABLE_FIRST)
        i = 1;
    if (i) {
        s32 swap = equipment[0];
        equipment[0] = equipment[1];
        equipment[1] = swap;
    }
}
