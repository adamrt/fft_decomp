#include "fft/event_bunit.h"
#include "fft/menu.h"
#include "psx/types.h"

s32 menu_set_selection_value(u8 index, s16 value) {
    return g_menu_selection_values[index] = value;
}
