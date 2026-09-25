#include "fft/event_equip.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Apply the vertical-pad direction to one of the
 * menu selection values, wrapping it within [0, entry_count). Same body as
 * equip_menu_update_wrapped_horizontal_selection with the other pair of buttons.
 */
s16 equip_menu_update_wrapped_vertical_selection(u16 entry_count, u8 selection_index, u16 input_mask) {
    s16* selection;
    s16 up_value;
    s16 down_value;
    s16 next_value;
    s16* selection_base;
    s32 index;

    if ((input_mask & PSX_PAD_UP) != 0) {
        index = selection_index & 0xff;
        selection_base = g_equip_menu_selection_values;
        selection = (s16*)((index * sizeof(s16)) + (u32)selection_base);
        up_value = *selection;
        if (up_value == 0) {
            next_value = entry_count - 1;
        } else {
            next_value = up_value - 1;
        }
        *selection = next_value;
    } else if ((input_mask & PSX_PAD_DOWN) != 0) {
        index = selection_index & 0xff;
        selection_base = g_equip_menu_selection_values;
        selection = (s16*)((index * sizeof(s16)) + (u32)selection_base);
        down_value = *selection;
        if (down_value < ((entry_count & 0xffff) - 1)) {
            next_value = down_value + 1;
        } else {
            next_value = 0;
        }
        *selection = next_value;
    }

    return g_equip_menu_selection_values[selection_index & 0xff];
}
