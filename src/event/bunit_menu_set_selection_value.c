#include "fft/bunit.h"
#include "psx/types.h"

s32 bunit_menu_set_selection_value(u8 index, s16 value) {
    return g_bunit_menu_selection_values[index] = value;
}
