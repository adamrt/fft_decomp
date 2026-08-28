#include "fft/bunit.h"
#include "psx/types.h"

/* Update a wrapped selection and request the supplied menu state on change. */
void bunit_menu_update_horizontal_selection_and_mark_change(
    u16 entry_count, u8 selection_index, s32 input_mask, u8 changed_state) {
    s16 previous_selection;
    s16 selection;

    selection_index &= 0xff;
    previous_selection = g_bunit_menu_selection_values[selection_index];
    /* The target passes input_mask without the definition's u16 narrowing. */
    selection = ((s16 (*)(u16, u8, s32))bunit_menu_update_wrapped_horizontal_selection)(
        entry_count, selection_index, input_mask);
    if (previous_selection != selection) {
        g_bunit_sound_queued_effect_id = changed_state;
    }
}
