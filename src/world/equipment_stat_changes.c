/* Move and jump increases, stat changes due to equipment and item stat deltas. */

#include "fft/data_ability.h"
#include "fft/data_item.h"
#include "fft/main_item.h"
#include "fft/menu_status.h"
#include "psx/types.h"

/*
 * Populate the Move and Jump bonuses granted by a movement ability.
 *
 * IDs 0x1E6-0x1E8 grant Move +1/+2/+3; 0x1E9-0x1EB grant Jump +1/+2/+3.
 * Other abilities leave both bonuses at 0.
 */
static void formation_ability_get_move_jump_bonus(s16 ability, unit_panel_record_t* out) {
    unit_panel_summary_t summary;

    equipment_clear_stat_preview(&summary, out);
    if (ability >= ABILITY_ID_MOVEMENT_MOVE_PLUS_1 && ability <= ABILITY_ID_MOVEMENT_MOVE_PLUS_3) {
        out->state.move = ability - (ABILITY_ID_MOVEMENT_MOVE_PLUS_1 - 1);
    } else {
        out->state.move = 0;
    }
    if (ability >= ABILITY_ID_MOVEMENT_JUMP_PLUS_1 && ability <= ABILITY_ID_MOVEMENT_JUMP_PLUS_3) {
        out->state.jump = ability - (ABILITY_ID_MOVEMENT_JUMP_PLUS_1 - 1);
    } else {
        out->state.jump = 0;
    }
}

/* Stat preview for swapping one movement ability for another: clears
 * `stat_diff` and stores the difference in the Move and Jump bonuses the two
 * abilities give. */
void formation_ability_diff_movement_stats(unit_panel_record_t* stat_diff, s32 old_ability, s32 new_ability) {
    unit_panel_summary_t summary;
    unit_panel_record_t old_stats;
    unit_panel_record_t new_stats;

    equipment_clear_stat_preview(&summary, stat_diff);
    formation_ability_get_move_jump_bonus(old_ability, &old_stats);
    formation_ability_get_move_jump_bonus(new_ability, &new_stats);
    stat_diff->state.move = new_stats.state.move - old_stats.state.move;
    stat_diff->state.jump = new_stats.state.jump - old_stats.state.jump;
}

/*
 * Extract an item's equipment stats and attribute bonuses for comparison.
 *
 * Clears the preview, then fills what the item's class provides: a weapon's
 * power and evade (into the left-weapon fields when `alternate` is nonzero,
 * else the right-weapon fields), a shield's or accessory's physical and magic
 * evade, or headgear and armor's HP and MP bonus. Every item then adds its
 * attribute set's Move, Jump, Speed, PA and MA bonuses. Invalid item IDs
 * leave the cleared preview unchanged.
 */
void item_collect_stat_deltas(s16 item_id, unit_panel_summary_t* summary, unit_panel_record_t* detail, s32 alternate) {
    s32 attribute;
    s32 secondary;
    u8 power;
    u8 evade;

    equipment_clear_stat_preview(summary, detail);
    item_id &= ITEM_ENTRY_ID_MASK;
    if (item_id >= 1 && item_id <= ITEM_ID_END - 1) {
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
