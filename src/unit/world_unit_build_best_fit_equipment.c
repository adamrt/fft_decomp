#include "fft/world.h"

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
