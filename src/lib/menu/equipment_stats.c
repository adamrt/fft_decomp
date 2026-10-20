/* Build the equipment preview’s item contributions, compare old and new
 * loadouts, and total weapon power and equipment evade for the unit panels. */

#include "fft/ability.h"
#include "fft/equipment.h"
#include "fft/formation_ability.h"
#include "fft/item.h"
#include "fft/item_menu.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "psx/types.h"

/* Clear the HP/MP preview bonuses and equipment-detail stats. */
void equipment_stat_preview_clear(unit_panel_summary_t* summary, unit_panel_record_t* detail) {
    summary->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus = 0;
    summary->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus = 0;
    detail->state.move = 0;
    detail->state.speed = 0;
    detail->state.jump = 0;
    detail->state.right_weapon_power = 0;
    detail->state.left_weapon_power = 0;
    detail->state.left_weapon_evade = 0;
    detail->state.right_weapon_evade = 0;
    detail->state.physical_attack = 0;
    detail->state.physical_class_evade = 0;
    detail->state.physical_shield_evade = 0;
    detail->state.physical_accessory_evade = 0;
    detail->state.magical_attack = 0;
    detail->state.magical_class_evade = 0;
    detail->state.magical_shield_evade = 0;
    detail->state.magical_accessory_evade = 0;
}

#ifdef FFT_WORLD
/* WORLD also previews movement-ability bonuses.
 *
 * Populate the Move and Jump bonuses granted by a movement ability.
 *
 * IDs 0x1E6-0x1E8 grant Move +1/+2/+3; 0x1E9-0x1EB grant Jump +1/+2/+3.
 * Other abilities leave both bonuses at 0.
 */
static void movement_bonus_build(s16 ability, unit_panel_record_t* out) {
    unit_panel_summary_t summary;

    equipment_stat_preview_clear(&summary, out);
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

    equipment_stat_preview_clear(&summary, stat_diff);
    movement_bonus_build(old_ability, &old_stats);
    movement_bonus_build(new_ability, &new_stats);
    stat_diff->state.move = new_stats.state.move - old_stats.state.move;
    stat_diff->state.jump = new_stats.state.jump - old_stats.state.jump;
}
#endif

/*
 * Extract an item's equipment stats and attribute bonuses for comparison.
 *
 * Clears the preview, then fills what the item's class provides: a weapon's
 * power and evade (into the left-weapon fields when `alternate` is nonzero,
 * else the right-weapon fields), a shield's or accessory's physical and magic
 * evade, or headgear and armor's HP and MP bonus. Every item then adds its
 * attribute set's Move, Jump, Speed, PA and MA bonuses. WORLD requires an
 * item ID in the item table; EQUIP accepts every positive low-ten-bit ID.
 */
void item_stat_preview_build(s16 raw_item, unit_panel_summary_t* summary, unit_panel_record_t* detail, s32 alternate) {
    s32 item_id;
    s32 attribute;
    s32 secondary;
    s32 power;
    s32 evade;

    equipment_stat_preview_clear(summary, detail);
    item_id = raw_item & ITEM_ENTRY_ID_MASK;
    if (item_id > 0
#ifdef FFT_WORLD
        /* EQUIP omits this upper-bound check. */
        && item_id <= ITEM_ID_END - 1
#endif
    ) {
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

static void stat_preview_subtract_scaled(
    unit_panel_record_t* out, const unit_panel_record_t* scaled, const unit_panel_record_t* base, s32 scale);

/* Compute the stat changes of swapping item_before for item_after in a slot: the HP and MP bonus
 * differences into delta and the equipment-detail differences into output. */
void equipment_swap_delta_calculate(
    unit_panel_record_t* output, unit_panel_summary_t* delta, s16 item_before, s16 item_after, s32 slot) {
    unit_panel_summary_t summary_before;
    unit_panel_summary_t summary_after;
    unit_panel_record_t detail_before;
    unit_panel_record_t detail_after;

    item_stat_preview_build(item_before, &summary_before, &detail_before, slot);
    item_stat_preview_build(item_after, &summary_after, &detail_after, slot);
    delta->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus = summary_after.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus
        - summary_before.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;
    delta->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus = summary_after.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus
        - summary_before.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;
    stat_preview_subtract_scaled(output, &detail_before, &detail_after, 1);
}

/* Store base minus scaled detail stats; inputs may alias the output. */
static void stat_preview_subtract_scaled(
    unit_panel_record_t* out, const unit_panel_record_t* scaled, const unit_panel_record_t* base, s32 scale) {
    out->state.move = base->state.move - (scale * scaled->state.move);
    out->state.speed = base->state.speed - (scale * scaled->state.speed);
    out->state.jump = base->state.jump - (scale * scaled->state.jump);
    out->state.right_weapon_power = base->state.right_weapon_power - (scale * scaled->state.right_weapon_power);
    out->state.left_weapon_power = base->state.left_weapon_power - (scale * scaled->state.left_weapon_power);
    out->state.right_weapon_evade = base->state.right_weapon_evade - (scale * scaled->state.right_weapon_evade);
    out->state.left_weapon_evade = base->state.left_weapon_evade - (scale * scaled->state.left_weapon_evade);
    out->state.physical_attack = base->state.physical_attack - (scale * scaled->state.physical_attack);
    out->state.physical_class_evade = base->state.physical_class_evade - (scale * scaled->state.physical_class_evade);
    out->state.physical_shield_evade
        = base->state.physical_shield_evade - (scale * scaled->state.physical_shield_evade);
    out->state.physical_accessory_evade
        = base->state.physical_accessory_evade - (scale * scaled->state.physical_accessory_evade);
    out->state.magical_attack = base->state.magical_attack - (scale * scaled->state.magical_attack);
    out->state.magical_class_evade = base->state.magical_class_evade - (scale * scaled->state.magical_class_evade);
    out->state.magical_shield_evade = base->state.magical_shield_evade - (scale * scaled->state.magical_shield_evade);
    out->state.magical_accessory_evade
        = base->state.magical_accessory_evade - (scale * scaled->state.magical_accessory_evade);
}

/* Sum the stat changes of swapping each of the five equipment slots from `from_items` to `to_items`. */
void equipment_loadout_delta_calculate(unit_panel_record_t* detail_total, unit_panel_summary_t* summary_total,
    const u16* from_items, const u16* to_items) {
    unit_panel_summary_t summary_delta;
    unit_panel_record_t detail_delta;
    unit_panel_record_t unused; /* unreferenced in the target; needed for the frame size */
    s32 i;

    equipment_stat_preview_clear(summary_total, detail_total);

    for (i = 0; i < 5; i++) {
        equipment_swap_delta_calculate(&detail_delta, &summary_delta, from_items[i] & 0x3FF, to_items[i] & 0x3FF, i);
        stat_preview_subtract_scaled(detail_total, detail_total, &detail_delta, -1);
        summary_total->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus += summary_delta.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;
        summary_total->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus += summary_delta.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;
    }
}

/* Sum weapon power and weapon, shield and accessory evade from the five equipped items.
 * Other fields in total are unchanged. */
void equipment_weapon_evade_totals_build(unit_panel_record_t* total, const u16* equipment) {
    unit_panel_summary_t summary;
    unit_panel_record_t detail;
    unit_panel_record_t unused; /* unreferenced in the target; needed for the frame size */
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
        item_stat_preview_build(equipment[i] & ITEM_ENTRY_ID_MASK, &summary, &detail, i);
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
