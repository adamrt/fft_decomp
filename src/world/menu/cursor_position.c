/* Storing a menu selection value to put a tab cursor on its first entry. */

#include "fft/menu_selection.h"
#include "fft/screen.h"
#include "psx/types.h"

/* The same routine as menu_selection_value_init, compiled twice. The list steps call this copy
 * as they start, to put a tab cursor on its first entry, and keep the result as their category. */
s16 screen_cursor_position_init(u8 index, s16 value) {
    g_menu_selection_values[index] = value;
    return value;
}
