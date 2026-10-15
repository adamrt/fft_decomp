#include "fft/world.h"
#include "psx/types.h"

/* Menu script callback 6 of the item category menu: the selected unit's previewed HP change. */
s32 world_menu_get_selected_unit_hp_bonus(s32 unused_row) {
    return g_menu_selected_unit_stat_summary.gauges[MENU_UNIT_STATUS_GAUGE_HP].bonus;
}
