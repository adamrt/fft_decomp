#include "fft/event_bunit.h"

s16 bunit_unit_get_current_stat(s32 unit_index, s32 value) {
    bunit_unit_stat_e mode;
    bunit_unit_data_t* entry;

    mode = g_bunit_unit_stat_mode;
    if (mode == BUNIT_UNIT_STAT_HP) {
        entry = g_bunit_unit_data[unit_index];
        value = entry->current_hp;
    } else if (mode == BUNIT_UNIT_STAT_MP) {
        entry = g_bunit_unit_data[unit_index];
        value = entry->current_mp;
    } else if (mode == BUNIT_UNIT_STAT_CT) {
        entry = g_bunit_unit_data[unit_index];
        value = entry->current_ct;
    }
    g_bunit_unit_current_stat_value = value;
    return (s16)value;
}
