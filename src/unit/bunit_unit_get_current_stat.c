#include "fft/event_bunit.h"

/* Return the listed unit's current HP, MP, or CT value, as
 * g_bunit_unit_stat_mode selects. For any other mode the value returned is
 * never assigned. */
s32 bunit_unit_get_current_stat(s32 unit_index) {
    menu_unit_readout_e mode;
    bunit_unit_t* entry;
    s32 value;

    mode = g_bunit_unit_stat_mode;
    if (mode == MENU_UNIT_READOUT_HP) {
        entry = g_bunit_unit_pointers[unit_index];
        value = entry->gauges[MENU_UNIT_STATUS_GAUGE_HP].value;
    } else if (mode == MENU_UNIT_READOUT_MP) {
        entry = g_bunit_unit_pointers[unit_index];
        value = entry->gauges[MENU_UNIT_STATUS_GAUGE_MP].value;
    } else if (mode == MENU_UNIT_READOUT_CT) {
        entry = g_bunit_unit_pointers[unit_index];
        value = entry->gauges[MENU_UNIT_STATUS_GAUGE_CT].value;
    }
    g_bunit_unit_current_stat_value = value;
    return g_bunit_unit_current_stat_value;
}
