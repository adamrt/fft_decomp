#include "fft/event_equip.h"

s32 equip_unit_set_slot_item(s16 unit_index, s16 slot, s32 item_id) {
    s16 value;
    equip_item_slot_result_e result;

    value = item_id & EQUIP_ITEM_ID_MASK;
    result = equip_unit_validate_slot_item(unit_index, slot, value);
    if (result < 0) {
        return result;
    }
    /* The typed equipment field folds 0x54 into the pointer expression and
     * changes register scheduling. This halfword view preserves the exact
     * target while indexing the same equip_unit_data_t.equipment array. */
    if (unit_index != 1) {
        equip_item_adjust_inventory_count(((s16*)g_equip_unit_data[unit_index])[slot + 42], 1);
    }
    if (slot < EQUIP_SLOT_HAND_COUNT && result != EQUIP_SLOT_ITEM_ALLOWED) {
        s32 other = (slot == EQUIP_SLOT_RIGHT_HAND);
        if (unit_index != 1) {
            equip_item_adjust_inventory_count(((s16*)g_equip_unit_data[unit_index])[other + 42], 1);
        }
        ((s16*)g_equip_unit_data[unit_index])[other + 42] = 0;
    }
    ((s16*)g_equip_unit_data[unit_index])[slot + 42] = value;
    if (unit_index != 1) {
        equip_unit_commit_loadout_to_battle_stats();
    }
    return 1;
}
