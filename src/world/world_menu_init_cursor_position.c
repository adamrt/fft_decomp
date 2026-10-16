#include "fft/menu_input.h"
#include "psx/types.h"

/* The same routine as menu_init_selection_value, compiled twice. The list steps call this copy
 * as they start, to put a tab cursor on its first entry, and keep the result as their category. */
s16 world_menu_init_cursor_position(u8 index, s16 value) {
    g_menu_selection_values[index] = value;
    return value;
}
