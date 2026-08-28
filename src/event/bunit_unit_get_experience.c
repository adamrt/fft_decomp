#include "fft/bunit.h"

s32 bunit_unit_get_experience(s32 unit_id) {
    return g_bunit_unit_data[unit_id]->experience;
}
