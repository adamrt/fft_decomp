#include "fft/event_equip.h"
#include "psx/types.h"

s32 equip_menu_get_preview_hp_bonus(s32 unused_row) {
    return g_menu_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;
}
