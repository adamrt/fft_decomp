#include "fft/bunit.h"

s32 bunit_unit_get_faith(s32 unit_id) {
    return g_bunit_unit_data[unit_id]->faith;
}
