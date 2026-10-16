/* Previewed HP and MP bonus display callbacks. */

#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/types.h"

s32 menu_get_preview_hp_bonus_display(s32 unused_row) {
    s32 value = g_menu_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;

    if (value == 0) {
        return MENU_NUMBER_VALUE_DASHES;
    }
    return value;
}

/* Menu callback: the previewed MP bonus, or dashes when it is zero. */
s32 menu_get_preview_mp_bonus_display(s32 unused_row) {
    s32 value = g_menu_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;

    if (value == 0) {
        return MENU_NUMBER_VALUE_DASHES;
    }
    return value;
}
