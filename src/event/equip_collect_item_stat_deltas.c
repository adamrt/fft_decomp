#include "fft/event_equip.h"

/*
 * Extract an item's equipment stats and attribute bonuses for comparison.
 *
 * Unlike the WORLD twin, this overlay accepts every positive low-10-bit item
 * ID; preserving that gate is required for the target control flow.
 */
void equip_collect_item_stat_deltas(
    s32 raw_item, world_item_stat_summary_t* summary, world_item_stat_detail_t* detail, s32 alternate) {
    s32 item_id;
    s32 attribute;
    s32 secondary;
    s32 lo;
    s32 hi;

    equip_item_clear_stat_sums((s16*)summary, (s16*)detail);
    item_id = raw_item & EQUIP_ITEM_ID_MASK;
    if (item_id > 0) {
        attribute = g_main_item_primary_data[item_id].attributes;
        if (item_id < ITEM_ID_SHIELD_FIRST) {
            secondary = g_main_item_primary_data[item_id].secondary_data_id;
            lo = g_main_item_weapon_data[secondary].power;
            hi = g_main_item_weapon_data[secondary].evade;
            if (alternate != 0) {
                detail->left_weapon_power = lo;
                detail->left_weapon_evade = hi;
            } else {
                detail->right_weapon_power = lo;
                detail->right_weapon_evade = hi;
            }
        } else if (item_id < ITEM_ID_HEADGEAR_FIRST) {
            detail->physical_shield_evade
                = g_main_item_shield_data[g_main_item_primary_data[item_id].secondary_data_id].physical_evade;
            detail->magical_shield_evade
                = g_main_item_shield_data[g_main_item_primary_data[item_id].secondary_data_id].magic_evade;
        } else if (item_id < ITEM_ID_ACCESSORY_FIRST) {
            summary->hp_bonus = g_main_item_helm_armor_secondary_data[item_id - ITEM_ID_HEADGEAR_FIRST].hp_bonus;
            summary->mp_bonus = g_main_item_helm_armor_secondary_data[item_id - ITEM_ID_HEADGEAR_FIRST].mp_bonus;
        } else if (item_id < ITEM_ID_CONSUMABLE_FIRST) {
            detail->physical_accessory_evade
                = g_main_item_accessory_data[g_main_item_primary_data[item_id].secondary_data_id].physical_evade;
            detail->magical_accessory_evade
                = g_main_item_accessory_data[g_main_item_primary_data[item_id].secondary_data_id].magic_evade;
        }
        detail->move_bonus = g_main_item_attributes[attribute].move;
        detail->speed_bonus = g_main_item_attributes[attribute].attributes[ITEM_ATTRIBUTE_SPEED];
        detail->jump_bonus = g_main_item_attributes[attribute].jump;
        detail->physical_attack_bonus = g_main_item_attributes[attribute].attributes[ITEM_ATTRIBUTE_PHYSICAL_ATTACK];
        detail->magical_attack_bonus = g_main_item_attributes[attribute].attributes[ITEM_ATTRIBUTE_MAGICAL_ATTACK];
    }
}
