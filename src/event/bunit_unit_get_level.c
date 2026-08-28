#include "fft/bunit.h"

s32 bunit_unit_get_level(s32 unit_id) {
    return g_bunit_unit_data[unit_id]->level;
}
