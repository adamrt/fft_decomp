#include "fft/equip.h"
#include "psx/types.h"

/* equip_stats_t is a 0x80-byte superset of the 0x40-byte stat record: every
 * halfword this function accumulates (0x06/0x08/0x0a/0x0c and
 * 0x16/0x18/0x20/0x22) sits at the offset fft/world.h gives
 * world_item_stat_detail_t, so the local view stays and only the detail
 * argument is cast. */
struct world_item_stat_summary;

/* Sum the stat bonuses of the five equipped items listed in `items`. */
void equip_unit_calculate_equipment_stat_bonuses(equip_stats_t* bonuses, u16* items) {
    u8 item_summary[0x28];
    equip_stats_t item_stats;
    int i;
    u16* item;

    bonuses->right_weapon_power = 0;
    bonuses->left_weapon_power = 0;
    bonuses->left_weapon_evade = 0;
    bonuses->right_weapon_evade = 0;
    bonuses->physical_shield_evade = 0;
    bonuses->physical_accessory_evade = 0;
    bonuses->magical_shield_evade = 0;
    bonuses->magical_accessory_evade = 0;

    for (i = 0, item = items; i < 5; i++) {
        equip_collect_item_stat_deltas(*item & 0x3FF, (struct world_item_stat_summary*)item_summary,
            (struct world_item_stat_detail*)&item_stats, i);
        bonuses->right_weapon_power += item_stats.right_weapon_power;
        bonuses->left_weapon_power += item_stats.left_weapon_power;
        bonuses->right_weapon_evade += item_stats.right_weapon_evade;
        bonuses->left_weapon_evade += item_stats.left_weapon_evade;
        bonuses->physical_shield_evade += item_stats.physical_shield_evade;
        bonuses->physical_accessory_evade += item_stats.physical_accessory_evade;
        bonuses->magical_shield_evade += item_stats.magical_shield_evade;
        bonuses->magical_accessory_evade += item_stats.magical_accessory_evade;
        item++;
    }
}
