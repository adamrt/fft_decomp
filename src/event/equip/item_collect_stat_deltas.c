/* An item's equipment stats and attribute bonuses, collected for comparison. */

#include "fft/equipment.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "psx/types.h"

/*
 * Extract an item's equipment stats and attribute bonuses for comparison.
 *
 * Unlike the WORLD twin, this overlay accepts every positive low-10-bit item
 * ID; preserving that gate is required for the target control flow.
 */
void item_collect_stat_deltas(s16 raw_item, unit_panel_summary_t* summary, unit_panel_record_t* detail, s32 alternate) {
    s32 item_id;
    s32 attribute;
    s32 secondary;
    s32 power;
    s32 evade;

    equipment_clear_stat_preview(summary, detail);
    item_id = raw_item & ITEM_ENTRY_ID_MASK;
    if (item_id > 0) {
        attribute = g_item_primary_data[item_id].attributes;
        if (item_id < ITEM_ID_SHIELD_FIRST) {
            secondary = g_item_primary_data[item_id].secondary_data_id;
            power = g_item_weapon_data[secondary].power;
            evade = g_item_weapon_data[secondary].evade;
            if (alternate != 0) {
                detail->state.left_weapon_power = power;
                detail->state.left_weapon_evade = evade;
            } else {
                detail->state.right_weapon_power = power;
                detail->state.right_weapon_evade = evade;
            }
        } else if (item_id < ITEM_ID_HEADGEAR_FIRST) {
            detail->state.physical_shield_evade
                = g_item_shield_data[g_item_primary_data[item_id].secondary_data_id].physical_evade;
            detail->state.magical_shield_evade
                = g_item_shield_data[g_item_primary_data[item_id].secondary_data_id].magic_evade;
        } else if (item_id < ITEM_ID_ACCESSORY_FIRST) {
            summary->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus
                = g_item_helm_armor_secondary_data[item_id - ITEM_ID_HEADGEAR_FIRST].hp_bonus;
            summary->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus
                = g_item_helm_armor_secondary_data[item_id - ITEM_ID_HEADGEAR_FIRST].mp_bonus;
        } else if (item_id < ITEM_ID_CONSUMABLE_FIRST) {
            detail->state.physical_accessory_evade
                = g_item_accessory_data[g_item_primary_data[item_id].secondary_data_id].physical_evade;
            detail->state.magical_accessory_evade
                = g_item_accessory_data[g_item_primary_data[item_id].secondary_data_id].magic_evade;
        }
        detail->state.move = g_item_attributes[attribute].move;
        detail->state.speed = g_item_attributes[attribute].attributes[UNIT_ATTRIBUTE_SPEED];
        detail->state.jump = g_item_attributes[attribute].jump;
        detail->state.physical_attack = g_item_attributes[attribute].attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
        detail->state.magical_attack = g_item_attributes[attribute].attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    }
}
