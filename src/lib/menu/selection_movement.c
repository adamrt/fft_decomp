/* Move a remembered menu cursor with directional wrapping and queue its change sound. */

#include "fft/menu_selection.h"
#include "fft/sound.h"
#include "psx/pad.h"
#include "psx/types.h"

#if !defined(FFT_EVENT_BUNIT)
/* WORLD and EQUIP have the vertical pair before the horizontal pair. */
/* Apply the vertical-pad direction to one of the menu selection values, wrapping it within [0, entry_count). */
s16 menu_selection_vertical_step(u16 entry_count, u8 selection_index, s32 input_mask) {
    if (input_mask & PSX_PAD_UP) {
        g_menu_selection_values[selection_index] = (g_menu_selection_values[selection_index] == 0)
            ? entry_count - 1
            : g_menu_selection_values[selection_index] - 1;
    } else if (input_mask & PSX_PAD_DOWN) {
        g_menu_selection_values[selection_index] = (g_menu_selection_values[selection_index] < entry_count - 1)
            ? g_menu_selection_values[selection_index] + 1
            : 0;
    }
    return g_menu_selection_values[selection_index];
}

/* Update a wrapping vertical menu selection, queue `sound_id` when it moves
 * and return the new selection. */
s32 menu_selection_vertical_sound_step(s32 entry_count, s32 selection_index, s32 input_mask, s8 sound_id) {
    s32 index = selection_index & 0xFF;
    s16 previous_selection = g_menu_selection_values[index];
    s16 selection;

    selection = menu_selection_vertical_step(entry_count, index, input_mask);
    if (previous_selection != selection) {
        g_sound_queued_effect_id = sound_id;
    }
    return selection;
}
#endif

/* Step selection value selection_index left or right on the D-pad, wrapping within entry_count,
 * and return it. */
s16 menu_selection_horizontal_step(u16 entry_count, u8 selection_index, s32 input_mask) {
    if (input_mask & PSX_PAD_LEFT) {
        g_menu_selection_values[selection_index] = (g_menu_selection_values[selection_index] == 0)
            ? entry_count - 1
            : g_menu_selection_values[selection_index] - 1;
    } else if (input_mask & PSX_PAD_RIGHT) {
        g_menu_selection_values[selection_index] = (g_menu_selection_values[selection_index] < entry_count - 1)
            ? g_menu_selection_values[selection_index] + 1
            : 0;
    }
    return g_menu_selection_values[selection_index];
}

/* Update a wrapping horizontal menu selection, queue `sound_id` when it moves
 * and return the new selection. */
s32 menu_selection_horizontal_sound_step(u16 entry_count, u8 selection_index, s32 input_mask, u8 sound_id) {
    s16 previous_selection;
    s16 selection;

    previous_selection = g_menu_selection_values[selection_index];
    selection = menu_selection_horizontal_step(entry_count, selection_index, input_mask);
    if (previous_selection != selection) {
        g_sound_queued_effect_id = sound_id;
    }
    return selection;
}
