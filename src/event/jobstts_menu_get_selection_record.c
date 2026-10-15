#include "fft/event_jobstts.h"

/*
 * Read back the cursor and scroll base saved for list `index` and check them
 * against the rebuilt -1-terminated `abilities`.
 *
 * They are kept when the saved entry id is still at the cursor row. Otherwise
 * the cursor moves to the row that holds that id (low ten bits), keeping the
 * scroll base, or both reset to 0 when the id is gone. Unlike the WORLD
 * original this copy does not check that the cursor row lies before the
 * terminator.
 */
void jobstts_menu_get_selection_record(
    s32 index, s16* out_selected_index, s16* out_scroll_index, const s16* abilities) {
    s32 ability_id;
    s32 i;

    *out_selected_index = g_menu_selection_records[index].selected_index;
    *out_scroll_index = g_menu_selection_records[index].scroll_base_index;
    ability_id = g_menu_selection_records[index].entry_id;
    if (ability_id != abilities[*out_selected_index]) {
        for (i = 0; abilities[i] != -1; i++) {
            if ((abilities[i] & 0x3ff) == ability_id) {
                *out_selected_index = i;
                return;
            }
        }
        *out_selected_index = 0;
        *out_scroll_index = 0;
    }
}
