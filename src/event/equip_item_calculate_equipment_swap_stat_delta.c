#include "fft/event_equip.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Sum the stat changes of swapping each of the five equipment slots from `from_items` to `to_items`. */
void equip_item_calculate_equipment_swap_stat_delta(menu_status_panel_slot_storage_t* detail_total,
    menu_unit_status_record_t* summary_total, const u16* from_items, const u16* to_items) {
    menu_unit_status_record_t summary_delta;
    menu_status_panel_slot_storage_t detail_delta;
    menu_status_panel_slot_storage_t unused; /* unreferenced in the target; needed for the frame size */
    s32 i;

    menu_clear_stat_preview(summary_total, detail_total);

    for (i = 0; i < 5; i++) {
        menu_calculate_swap_stat_delta(&detail_delta, &summary_delta, from_items[i] & 0x3FF, to_items[i] & 0x3FF, i);
        menu_subtract_scaled_stat_preview(detail_total, detail_total, &detail_delta, -1);
        summary_total->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus += summary_delta.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;
        summary_total->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus += summary_delta.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;
    }
}
