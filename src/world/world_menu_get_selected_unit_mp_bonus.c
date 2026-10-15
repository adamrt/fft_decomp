#include "fft/world.h"
#include "psx/types.h"

/* Menu script callback 7 of the item category menu: the selected unit's previewed MP change. */
s32 world_menu_get_selected_unit_mp_bonus(s32 unused_row) {
    return g_menu_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_MP].bonus;
}
