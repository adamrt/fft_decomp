#include "fft/menu_input.h"
#include "fft/sound.h"
#include "psx/types.h"

/* Update a wrapping vertical menu selection, queue `sound_id` when it moves
 * and return the new selection. */
s32 menu_update_vertical_selection_and_mark_change(s32 entry_count, s32 selection_index, s32 input_mask, s8 sound_id) {
    s32 index = selection_index & 0xFF;
    s16 previous_selection = g_menu_selection_values[index];
    s16 selection;

    selection = menu_update_wrapped_vertical_selection(entry_count & 0xFFFF, index, input_mask);
    if (previous_selection != selection) {
        g_sound_queued_effect_id = sound_id;
    }
    return selection;
}
