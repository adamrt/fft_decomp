#include "fft/event_equip.h"
#include "psx/types.h"

s32 equip_menu_set_selection_value_2(s32 index, s32 value) {
    g_equip_menu_selection_values[(u8)index] = value;
    return (s16)value;
}
