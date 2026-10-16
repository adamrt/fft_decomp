/* The selection record lookup and cycling the selected unit. */

#include "fft/formation.h"
#include "fft/menu_selection.h"
#include "fft/sound.h"
#include "fft/unit.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Read back the cursor and scroll base saved for list `index` and check them
 * against the rebuilt -1-terminated `entries`.
 *
 * They are kept when the saved entry id is still at the cursor row. Otherwise
 * the cursor moves to the row that holds that id (low ten bits), keeping the
 * scroll base, or both reset to 0 when the id is gone.
 */
void menu_get_selection_record(s32 index, s16* selected_index, s16* scroll_base_index, s16* entries) {
    s32 entry_id;
    s32 found_end;
    s32 i;

    *selected_index = g_menu_selection_records[index].selected_index;
    *scroll_base_index = g_menu_selection_records[index].scroll_base_index;
    entry_id = g_menu_selection_records[index].entry_id;
    found_end = 0;
    for (i = 0; i < *selected_index; i++) {
        if (entries[i] == -1) {
            found_end = 1;
        }
    }
    if (entry_id == entries[*selected_index] && found_end == 0) {
        return;
    }
    for (i = 0; entries[i] != -1; i++) {
        if ((entries[i] & 0x3ff) == entry_id) {
            *selected_index = i;
            return;
        }
    }
    *selected_index = 0;
    *scroll_base_index = 0;
}

/* Steps the formation roster cursor one slot backwards (L1) or forwards (R1),
 * wrapping at either end and skipping over egg units. Plays the page-switch
 * sound on either move. */
void formation_cycle_selected_unit(s16 pad_buttons) {
    u16 index;

    if (pad_buttons & PSX_PAD_L1) {
        g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
        do {
            if (g_unit_selected_index == 0) {
                index = g_formation_unit_count - 1;
            } else {
                index = g_unit_selected_index - 1;
            }
            g_unit_selected_index = index;
        } while (g_formation_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG);
    } else if (pad_buttons & PSX_PAD_R1) {
        g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
        do {
            index = (g_unit_selected_index != g_formation_unit_count - 1) ? g_unit_selected_index + 1 : 0;
            g_unit_selected_index = index;
        } while (g_formation_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG);
    }
}
