/* Stat deltas of swapping equipment and the scaled preview subtraction. */

#include "fft/data_item.h"
#include "fft/item.h"
#include "fft/menu_status.h"
#include "psx/types.h"

static void equipment_subtract_scaled_stat_preview(
    unit_panel_record_t* out, const unit_panel_record_t* scaled, const unit_panel_record_t* base, s32 scale);

/* Compute the stat changes of swapping item_before for item_after in a slot: the HP and MP bonus
 * differences into delta and the equipment-detail differences into output. */
void equipment_calculate_swap_stat_delta(
    unit_panel_record_t* output, unit_panel_summary_t* delta, s16 item_before, s16 item_after, s32 slot) {
    unit_panel_summary_t summary_before;
    unit_panel_summary_t summary_after;
    unit_panel_record_t detail_before;
    unit_panel_record_t detail_after;

    item_collect_stat_deltas(item_before, &summary_before, &detail_before, slot);
    item_collect_stat_deltas(item_after, &summary_after, &detail_after, slot);
    delta->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus = summary_after.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus
        - summary_before.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;
    delta->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus = summary_after.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus
        - summary_before.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;
    equipment_subtract_scaled_stat_preview(output, &detail_before, &detail_after, 1);
}

/* Store base minus scaled detail stats; inputs may alias the output. */
static void equipment_subtract_scaled_stat_preview(
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
void equipment_calculate_total_swap_delta(unit_panel_record_t* detail_total, unit_panel_summary_t* summary_total,
    const u16* from_items, const u16* to_items) {
    unit_panel_summary_t summary_delta;
    unit_panel_record_t detail_delta;
    unit_panel_record_t unused; /* unreferenced in the target; needed for the frame size */
    s32 i;

    equipment_clear_stat_preview(summary_total, detail_total);

    for (i = 0; i < 5; i++) {
        equipment_calculate_swap_stat_delta(
            &detail_delta, &summary_delta, from_items[i] & 0x3FF, to_items[i] & 0x3FF, i);
        equipment_subtract_scaled_stat_preview(detail_total, detail_total, &detail_delta, -1);
        summary_total->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus += summary_delta.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;
        summary_total->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus += summary_delta.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;
    }
}

/* Sum the stat bonuses of the five equipped items listed in `equipment`. */
void equipment_sum_stat_details(unit_panel_record_t* total, const u16* equipment) {
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
        item_collect_stat_deltas(equipment[i] & ITEM_ENTRY_ID_MASK, &summary, &detail, i);
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
