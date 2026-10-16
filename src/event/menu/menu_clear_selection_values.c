/* Clearing every saved menu selection value. */

#include "fft/menu_input.h"
#include "psx/types.h"

/* Clear every saved menu selection value. The table has
 * VARIANT_MENU_SELECTION_VALUE_COUNT entries: 16 in BUNIT and WORLD, 4 in
 * EQUIP, 8 in JOBSTTS. */
void menu_clear_selection_values(void) {
    s32 index;

    for (index = VARIANT_MENU_SELECTION_VALUE_COUNT - 1; index >= 0; index--) {
        g_menu_selection_values[index] = 0;
    }
}
