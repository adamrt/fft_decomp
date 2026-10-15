#include "fft/world.h"
#include "psx/types.h"

s16 world_menu_set_cursor_position(u8 index, s16 value) {
    g_menu_selection_values[index] = value;
    return value;
}
