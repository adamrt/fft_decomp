/* An item's equipment stats and attribute bonuses, collected for comparison. */

#include "fft/data_item.h"
#include "fft/main_item.h"
#include "fft/menu.h"
#include "fft/menu_panel.h"
#include "psx/types.h"

/*
 * Extract an item's equipment stats and attribute bonuses for comparison.
 *
 * Unlike the WORLD twin, this overlay accepts every positive low-10-bit item
 * ID; preserving that gate is required for the target control flow.
 */
void item_collect_stat_deltas_variant(
    s16 raw_item, menu_unit_status_record_t* summary, menu_status_panel_slot_storage_t* detail, s32 alternate) {
    s32 item_id;
    s32 attribute;
    s32 secondary;
    s32 lo;
    s32 hi;

    menu_clear_stat_preview(summary, detail);
    item_id = raw_item & ITEM_ENTRY_ID_MASK;
    if (item_id > 0) {
        attribute = g_main_item_primary_data[item_id].attributes;
        if (item_id < ITEM_ID_SHIELD_FIRST) {
            secondary = g_main_item_primary_data[item_id].secondary_data_id;
            lo = g_main_item_weapon_data[secondary].power;
            hi = g_main_item_weapon_data[secondary].evade;
            if (alternate != 0) {
                detail->state.left_weapon_power = lo;
                detail->state.left_weapon_evade = hi;
            } else {
                detail->state.right_weapon_power = lo;
                detail->state.right_weapon_evade = hi;
            }
        } else if (item_id < ITEM_ID_HEADGEAR_FIRST) {
            detail->state.physical_shield_evade
                = g_main_item_shield_data[g_main_item_primary_data[item_id].secondary_data_id].physical_evade;
            detail->state.magical_shield_evade
                = g_main_item_shield_data[g_main_item_primary_data[item_id].secondary_data_id].magic_evade;
        } else if (item_id < ITEM_ID_ACCESSORY_FIRST) {
            summary->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus
                = g_main_item_helm_armor_secondary_data[item_id - ITEM_ID_HEADGEAR_FIRST].hp_bonus;
            summary->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus
                = g_main_item_helm_armor_secondary_data[item_id - ITEM_ID_HEADGEAR_FIRST].mp_bonus;
        } else if (item_id < ITEM_ID_CONSUMABLE_FIRST) {
            detail->state.physical_accessory_evade
                = g_main_item_accessory_data[g_main_item_primary_data[item_id].secondary_data_id].physical_evade;
            detail->state.magical_accessory_evade
                = g_main_item_accessory_data[g_main_item_primary_data[item_id].secondary_data_id].magic_evade;
        }
        detail->state.move = g_main_item_attributes[attribute].move;
        detail->state.speed = g_main_item_attributes[attribute].attributes[UNIT_ATTRIBUTE_SPEED];
        detail->state.jump = g_main_item_attributes[attribute].jump;
        detail->state.physical_attack = g_main_item_attributes[attribute].attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
        detail->state.magical_attack = g_main_item_attributes[attribute].attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    }
}
