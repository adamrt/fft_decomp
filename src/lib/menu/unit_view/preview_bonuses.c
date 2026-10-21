/* Previewed HP and MP bonus display callbacks. */

#include "fft/equipment.h"
#include "fft/menu_script.h"
#include "fft/unit_panel.h"
#include "psx/types.h"

/* Row callback: return the previewed HP bonus, or MENU_NUMBER_VALUE_DASHES to draw dashes when it
 * is 0; the row is ignored. */
s32 equipment_get_preview_hp_bonus_display(s32 unused_row) {
    s32 value = g_unit_panel_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;

    if (value == 0) {
        return MENU_NUMBER_VALUE_DASHES;
    }
    return value;
}

/* Row callback: return the previewed MP bonus, or MENU_NUMBER_VALUE_DASHES to draw dashes when it
 * is 0; the row is ignored. */
s32 equipment_get_preview_mp_bonus_display(s32 unused_row) {
    s32 value = g_unit_panel_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;

    if (value == 0) {
        return MENU_NUMBER_VALUE_DASHES;
    }
    return value;
}
