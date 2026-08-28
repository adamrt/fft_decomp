#include "fft/bunit.h"

/* Return the selected unit's maximum HP, MP, or CT value. */
s16 bunit_unit_get_max_stat(s32 unit_index, s32 value) {
    if (g_bunit_unit_stat_mode == BUNIT_UNIT_STAT_HP) {
        value = g_bunit_unit_data[unit_index]->max_hp;
    } else if (g_bunit_unit_stat_mode == BUNIT_UNIT_STAT_MP) {
        value = g_bunit_unit_data[unit_index]->max_mp;
    } else if (g_bunit_unit_stat_mode == BUNIT_UNIT_STAT_CT) {
        value = 100;
    }
    g_bunit_unit_max_stat_value = value;
    return value;
}
