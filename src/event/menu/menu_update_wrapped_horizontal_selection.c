/* Stepping a menu selection value left or right, wrapping within its entry count. */

#include "fft/menu_input.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Step selection value selection_index left or right on the D-pad, wrapping within entry_count,
 * and return it. */
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
