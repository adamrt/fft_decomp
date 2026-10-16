/* Storing a menu selection value. */

#include "fft/menu_input.h"
#include "psx/types.h"

/* Store a menu selection value and return it. */
s32 menu_set_selection_value(u8 index, s16 value) {
    return g_menu_selection_values[index] = value;
}
