#include "fft/equip.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Apply the horizontal-pad direction to one of the menu selection values,
 * wrapping it within [0, entry_count).
 */
s16 equip_menu_update_wrapped_horizontal_selection(u16 entry_count, u8 selection_index, u16 input_mask) {
    s16* selection;
    s16 left_value;
    s16 right_value;
    s16 next_value;
    s16* selection_base;
    s32 index;

    if ((input_mask & PSX_PAD_LEFT) != 0) {
        index = selection_index & 0xff;
        selection_base = g_equip_menu_selection_values;
        selection = (s16*)((index * sizeof(s16)) + (u32)selection_base);
        left_value = *selection;
        if (left_value == 0) {
            next_value = entry_count - 1;
        } else {
            next_value = left_value - 1;
        }
        *selection = next_value;
    } else if ((input_mask & PSX_PAD_RIGHT) != 0) {
        index = selection_index & 0xff;
        selection_base = g_equip_menu_selection_values;
        selection = (s16*)((index * sizeof(s16)) + (u32)selection_base);
        right_value = *selection;
        if (right_value < ((entry_count & 0xffff) - 1)) {
            next_value = right_value + 1;
        } else {
            next_value = 0;
        }
        *selection = next_value;
    }

    return g_equip_menu_selection_values[selection_index & 0xff];
}
