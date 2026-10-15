#include "fft/menu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Apply the horizontal-pad direction to one of the menu selection values, wrapping it within [0, entry_count). */
s16 menu_update_wrapped_horizontal_selection(u16 entry_count, u8 selection_index, s32 input_mask) {
    if (input_mask & PSX_PAD_LEFT) {
        g_menu_selection_values[selection_index] = (g_menu_selection_values[selection_index] == 0)
            ? entry_count - 1
            : g_menu_selection_values[selection_index] - 1;
    } else if (input_mask & PSX_PAD_RIGHT) {
        g_menu_selection_values[selection_index] = (g_menu_selection_values[selection_index] < entry_count - 1)
            ? g_menu_selection_values[selection_index] + 1
            : 0;
    }
    return g_menu_selection_values[selection_index];
}
