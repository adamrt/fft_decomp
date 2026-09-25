#include "fft/event_bunit.h"

s32 bunit_unit_get_brave(s32 unit_id) {
    return g_bunit_unit_data[unit_id]->brave;
}
