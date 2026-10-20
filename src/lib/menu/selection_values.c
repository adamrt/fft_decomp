/* Remember menu cursor values and initialize or replace them when a list opens. */

#include "fft/menu_selection.h"
#include "psx/types.h"

/* Clear every saved menu selection value. The table has
 * VARIANT_MENU_SELECTION_VALUE_COUNT entries: 16 in BUNIT and WORLD, 4 in
 * EQUIP, 8 in JOBSTTS. */
void menu_selection_values_clear(void) {
    s32 index;

    for (index = VARIANT_MENU_SELECTION_VALUE_COUNT - 1; index >= 0; index--) {
        g_menu_selection_values[index] = 0;
    }
}

#if defined(FFT_EVENT_BUNIT) || defined(FFT_EVENT_EQUIP)
/* BUNIT and EQUIP keep a separate setter beside the selection reset. */
/* Store a menu selection value and return it. */
s32 menu_selection_value_set(u8 index, s16 value) {
    return g_menu_selection_values[index] = value;
}
#endif

#if defined(FFT_WORLD) || defined(FFT_EVENT_EQUIP)
/* WORLD and EQUIP retain this initializer as a distinct native entry. */
/* The same routine as menu_selection_value_set, compiled twice. formation_items_update
 * calls this copy as the menu opens and keeps the result as the selected slot. */
s32 menu_selection_value_init(u8 index, s16 value) {
    g_menu_selection_values[index] = value;
    return value;
}
#endif
