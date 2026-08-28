#include "fft/world.h"
#include "psx/types.h"

/* Sums the per-item stat details of the five equipment slots into total. */
void world_item_sum_equipment_stat_details(world_item_stat_detail_t* total, u16* equipment) {
    world_item_stat_summary_t summary;
    world_item_stat_detail_t detail;
    world_item_stat_detail_t unused; /* unreferenced in the target; needed for the 0xc8 frame */
    s32 i;

    total->right_weapon_power = 0;
    total->left_weapon_power = 0;
    total->left_weapon_evade = 0;
    total->right_weapon_evade = 0;
    total->physical_shield_evade = 0;
    total->physical_accessory_evade = 0;
    total->magical_shield_evade = 0;
    total->magical_accessory_evade = 0;
    for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
        world_item_populate_stat_preview(equipment[i] & WORLD_ITEM_ID_MASK, &summary, &detail, i);
        total->right_weapon_power += detail.right_weapon_power;
        total->left_weapon_power += detail.left_weapon_power;
        total->right_weapon_evade += detail.right_weapon_evade;
        total->left_weapon_evade += detail.left_weapon_evade;
        total->physical_shield_evade += detail.physical_shield_evade;
        total->physical_accessory_evade += detail.physical_accessory_evade;
        total->magical_shield_evade += detail.magical_shield_evade;
        total->magical_accessory_evade += detail.magical_accessory_evade;
    }
}
