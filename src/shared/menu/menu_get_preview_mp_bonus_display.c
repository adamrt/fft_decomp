#include "fft/event_equip.h"
#include "psx/types.h"

/* Menu callback: the previewed MP bonus, or dashes when it is zero. */
s32 menu_get_preview_mp_bonus_display(s32 unused_row) {
    s32 value = g_menu_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;

    if (value == 0) {
        return MENU_NUMBER_VALUE_DASHES;
    }
    return value;
}
