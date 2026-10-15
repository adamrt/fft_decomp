#include "fft/event_equip.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Load the selected unit into the EQUIP panels: clamp its HP and MP to their
 * maximums, copy its stat summary, identity and stat detail blocks, add the
 * equipment's stat details, copy the unit to compare slot 1 and reset the
 * menu state flags. */
void equip_unit_load_selected_data(void) {
    u8 unused[0x80];
    equip_unit_data_t* unit;

    unit = g_equip_unit_data[g_equip_unit_selected_index];
    if (unit->gauges[MENU_UNIT_STATUS_GAUGE_HP].max < unit->gauges[MENU_UNIT_STATUS_GAUGE_HP].value) {
        unit->gauges[MENU_UNIT_STATUS_GAUGE_HP].value = unit->gauges[MENU_UNIT_STATUS_GAUGE_HP].max;
    }

    unit = g_equip_unit_data[g_equip_unit_selected_index];
    if (unit->gauges[MENU_UNIT_STATUS_GAUGE_MP].max < unit->gauges[MENU_UNIT_STATUS_GAUGE_MP].value) {
        unit->gauges[MENU_UNIT_STATUS_GAUGE_MP].value = unit->gauges[MENU_UNIT_STATUS_GAUGE_MP].max;
    }

    bcopy(g_equip_unit_data[g_equip_unit_selected_index], &g_menu_selected_unit_stat_summary, 0x22);
    bcopy(&g_equip_unit_data[g_equip_unit_selected_index]->identity, &g_equip_panel_selected_unit_data,
        sizeof(menu_unit_status_identity_t));
    bcopy(&g_equip_unit_data[g_equip_unit_selected_index]->move, &g_menu_selected_unit_fields, 0x40);
    item_sum_equipment_stat_details(
        &g_menu_selected_unit_fields, g_equip_unit_data[g_equip_unit_selected_index]->equipment);
    equip_unit_copy_data_to_compare_slot(g_equip_unit_selected_index, 1);
    equip_menu_reset_state_flags();
}
