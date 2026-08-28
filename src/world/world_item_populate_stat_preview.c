#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/world.h"

extern item_data_t g_main_item_primary_data[];
extern shield_data_t g_main_item_shield_data[];
extern accessory_data_t g_main_item_accessory_data[];
extern armor_data_t g_main_item_helm_armor_secondary_data[];

/*
 * Extract an item's equipment stats and attribute bonuses for comparison.
 *
 * The right-hand slot selects the right-weapon fields; other slots select the
 * left-weapon fields.
 * Invalid item IDs leave the cleared preview unchanged.
 */
void world_item_populate_stat_preview(
    s32 g_main_item_item_flags, world_item_stat_summary_t* summary, world_item_stat_detail_t* detail, s32 slot) {
    s32 attribute;
    s32 secondary;
    u8 power;
    u8 evade;
    world_formation_clear_stat_preview(summary, detail);
    g_main_item_item_flags &= WORLD_ITEM_ID_MASK;
    if ((u32)(g_main_item_item_flags - 1) < 0xFD) {
        attribute = g_main_item_primary_data[g_main_item_item_flags].attributes;
        if (g_main_item_item_flags < ITEM_ID_SHIELD_FIRST) {
            secondary = g_main_item_primary_data[g_main_item_item_flags].secondary_data_id;
            power = g_main_item_weapon_data[secondary].power;
            evade = g_main_item_weapon_data[secondary].evade;
            if (slot != 0) {
                detail->left_weapon_power = power;
                detail->left_weapon_evade = evade;
            } else {
                detail->right_weapon_power = power;
                detail->right_weapon_evade = evade;
            }
        } else if (g_main_item_item_flags < ITEM_ID_HEADGEAR_FIRST) {
            detail->physical_shield_evade
                = g_main_item_shield_data[g_main_item_primary_data[g_main_item_item_flags].secondary_data_id]
                      .physical_evade;
            detail->magical_shield_evade
                = g_main_item_shield_data[g_main_item_primary_data[g_main_item_item_flags].secondary_data_id]
                      .magic_evade;
        } else if (g_main_item_item_flags < ITEM_ID_ACCESSORY_FIRST) {
            summary->hp_bonus
                = g_main_item_helm_armor_secondary_data[g_main_item_item_flags - ITEM_ID_HEADGEAR_FIRST].hp_bonus;
            summary->mp_bonus
                = g_main_item_helm_armor_secondary_data[g_main_item_item_flags - ITEM_ID_HEADGEAR_FIRST].mp_bonus;
        } else if (g_main_item_item_flags < ITEM_ID_CONSUMABLE_FIRST) {
            detail->physical_accessory_evade
                = g_main_item_accessory_data[g_main_item_primary_data[g_main_item_item_flags].secondary_data_id]
                      .physical_evade;
            detail->magical_accessory_evade
                = g_main_item_accessory_data[g_main_item_primary_data[g_main_item_item_flags].secondary_data_id]
                      .magic_evade;
        }
        detail->move_bonus = g_main_item_attributes[attribute].move;
        detail->speed_bonus = g_main_item_attributes[attribute].attributes[ITEM_ATTRIBUTE_SPEED];
        detail->jump_bonus = g_main_item_attributes[attribute].jump;
        detail->physical_attack_bonus = g_main_item_attributes[attribute].attributes[ITEM_ATTRIBUTE_PHYSICAL_ATTACK];
        detail->magical_attack_bonus = g_main_item_attributes[attribute].attributes[ITEM_ATTRIBUTE_MAGICAL_ATTACK];
    }
}
