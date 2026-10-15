#include "fft/event_equip.h"
#include "psx/types.h"

/* Sum the stat bonuses of the five equipped items listed in `equipment`. */
void item_sum_equipment_stat_details(menu_status_panel_slot_storage_t* total, const u16* equipment) {
    menu_unit_status_record_t summary;
    menu_status_panel_slot_storage_t detail;
    menu_status_panel_slot_storage_t unused; /* unreferenced in the target; needed for the frame size */
    s32 i;

    total->state.right_weapon_power = 0;
    total->state.left_weapon_power = 0;
    total->state.left_weapon_evade = 0;
    total->state.right_weapon_evade = 0;
    total->state.physical_shield_evade = 0;
    total->state.physical_accessory_evade = 0;
    total->state.magical_shield_evade = 0;
    total->state.magical_accessory_evade = 0;
    for (i = 0; i < 5; i++) {
        item_collect_stat_deltas_variant(equipment[i] & EQUIP_ITEM_ID_MASK, &summary, &detail, i);
        total->state.right_weapon_power += detail.state.right_weapon_power;
        total->state.left_weapon_power += detail.state.left_weapon_power;
        total->state.right_weapon_evade += detail.state.right_weapon_evade;
        total->state.left_weapon_evade += detail.state.left_weapon_evade;
        total->state.physical_shield_evade += detail.state.physical_shield_evade;
        total->state.physical_accessory_evade += detail.state.physical_accessory_evade;
        total->state.magical_shield_evade += detail.state.magical_shield_evade;
        total->state.magical_accessory_evade += detail.state.magical_accessory_evade;
    }
}
