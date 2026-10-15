#include "fft/menu.h"
#include "fft/world.h"

/*
 * Extract an item's equipment stats and attribute bonuses for comparison.
 *
 * The right-hand slot selects the right-weapon fields; other slots select the
 * left-weapon fields.
 * Invalid item IDs leave the cleared preview unchanged.
 */
void item_collect_stat_deltas_variant(
    s16 item_id, menu_unit_status_record_t* summary, menu_status_panel_slot_storage_t* detail, s32 slot) {
    s32 attribute;
    s32 secondary;
    u8 power;
    u8 evade;
    menu_clear_stat_preview(summary, detail);
    item_id &= WORLD_ITEM_ID_MASK;
    if (item_id >= 1 && item_id <= ITEM_ID_END - 1) {
        attribute = g_main_item_primary_data[item_id].attributes;
        if (item_id < ITEM_ID_SHIELD_FIRST) {
            secondary = g_main_item_primary_data[item_id].secondary_data_id;
            power = g_main_item_weapon_data[secondary].power;
            evade = g_main_item_weapon_data[secondary].evade;
            if (slot != 0) {
                detail->state.left_weapon_power = power;
                detail->state.left_weapon_evade = evade;
            } else {
                detail->state.right_weapon_power = power;
                detail->state.right_weapon_evade = evade;
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
        detail->state.speed = g_main_item_attributes[attribute].attributes[ITEM_ATTRIBUTE_SPEED];
        detail->state.jump = g_main_item_attributes[attribute].jump;
        detail->state.physical_attack = g_main_item_attributes[attribute].attributes[ITEM_ATTRIBUTE_PHYSICAL_ATTACK];
        detail->state.magical_attack = g_main_item_attributes[attribute].attributes[ITEM_ATTRIBUTE_MAGICAL_ATTACK];
    }
}
