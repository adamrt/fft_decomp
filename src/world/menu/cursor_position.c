/* Storing a menu selection value to put a tab cursor on its first entry. */

#include "fft/menu_selection.h"
#include "psx/types.h"

/* WORLD's copy of BUNIT's and EQUIP's setter, beside the library's menu_selection_init_value. The list steps call it as
 * they start, to put a tab cursor on its first entry, and keep the result as their category. */
s32 menu_selection_set_value(u8 index, s16 value) {
    return g_menu_selection_values[index] = value;
}
