/* A wrapping horizontal selection that queues a sound when it moves. */

#include "fft/menu_input.h"
#include "fft/sound.h"
#include "psx/types.h"

/* Update a wrapped selection, request the supplied menu state on change and
 * return the new selection. */
s32 menu_update_horizontal_selection_and_mark_change(
    u16 entry_count, u8 selection_index, s32 input_mask, u8 changed_state) {
    s16 previous_selection;
    s16 selection;

    selection_index &= 0xff;
    previous_selection = g_menu_selection_values[selection_index];
    selection = menu_update_wrapped_horizontal_selection(entry_count, selection_index, input_mask);
    if (previous_selection != selection) {
        g_sound_queued_effect_id = changed_state;
    }
    return selection;
}
