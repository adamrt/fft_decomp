#include "fft/world.h"

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
