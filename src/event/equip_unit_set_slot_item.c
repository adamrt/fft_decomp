#include "fft/event_equip.h"

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
