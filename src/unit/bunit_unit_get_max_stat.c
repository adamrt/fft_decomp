#include "fft/event_bunit.h"

/* Return the selected unit's maximum HP, MP, or CT value. */
s32 bunit_unit_get_max_stat(s32 unit_index) {
    s32 value;

    if (g_bunit_unit_stat_mode == MENU_UNIT_READOUT_HP) {
        value = g_bunit_unit_pointers[unit_index]->gauges[MENU_UNIT_STATUS_GAUGE_HP].max;
    } else if (g_bunit_unit_stat_mode == MENU_UNIT_READOUT_MP) {
        value = g_bunit_unit_pointers[unit_index]->gauges[MENU_UNIT_STATUS_GAUGE_MP].max;
    } else if (g_bunit_unit_stat_mode == MENU_UNIT_READOUT_CT) {
        value = UNIT_TURN_CT_THRESHOLD;
    }
    g_bunit_unit_max_stat_value = value;
    return g_bunit_unit_max_stat_value;
}
