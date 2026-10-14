#include "fft/menu.h"
#include "fft/world.h"
#include "psx/types.h"

/* Accumulates the stat deltas of all equipment slot changes into total. */
void world_item_calculate_equipment_swap_stat_delta(menu_status_panel_slot_storage_t* detail_total,
    menu_unit_status_record_t* total, const u16* items_before, const u16* items_after) {
    menu_unit_status_record_t delta;
    menu_status_panel_slot_storage_t detail;
    menu_status_panel_slot_storage_t unused;
    s32 slot;

    menu_clear_stat_preview(total, detail_total);
    for (slot = 0; slot < WORLD_EQUIPMENT_SLOT_COUNT; slot++) {
        menu_calculate_swap_stat_delta(
            &detail, &delta, items_before[slot] & WORLD_ITEM_ID_MASK, items_after[slot] & WORLD_ITEM_ID_MASK, slot);
        menu_subtract_scaled_stat_preview(detail_total, detail_total, &detail, -1);
        total->gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus += delta.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;
        total->gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus += delta.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;
    }
}
