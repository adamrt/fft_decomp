#include "fft/event_equip.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Compute the HP/MP and detailed stat changes for one equipment replacement. */
void menu_calculate_swap_stat_delta(menu_status_panel_slot_storage_t* output, menu_unit_status_record_t* delta,
    s16 item_before, s16 item_after, s32 slot) {
    menu_unit_status_record_t summary_before;
    menu_unit_status_record_t summary_after;
    menu_status_panel_slot_storage_t detail_before;
    menu_status_panel_slot_storage_t detail_after;

    item_collect_stat_deltas_variant(item_before, &summary_before, &detail_before, slot);
    item_collect_stat_deltas_variant(item_after, &summary_after, &detail_after, slot);
    delta->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus = summary_after.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus
        - summary_before.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;
    delta->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus = summary_after.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus
        - summary_before.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;
    menu_subtract_scaled_stat_preview(output, &detail_before, &detail_after, 1);
}
