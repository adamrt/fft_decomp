/* Restoring a list's saved cursor and scroll base, checked against the rebuilt entries. */

#include "fft/menu_selection.h"
#include "psx/types.h"

/*
 * Read back the cursor and scroll base saved for list `index` and check
 * them against the rebuilt -1-terminated `entries`.
 *
 * They are kept when the saved entry id is still at the cursor row. Otherwise
 * the cursor moves to the row that holds that id (low ten bits), keeping the
 * scroll base, or both reset to 0 when the id is gone.
 */
void menu_get_selection_record(s32 index, s16* selected_index, s16* scroll_base_index, s16* entries) {
    s32 i;
    s32 found_end = 0;
    s32 entry_id;

    *selected_index = g_menu_selection_records[index].selected_index;
    *scroll_base_index = g_menu_selection_records[index].scroll_base_index;
    entry_id = g_menu_selection_records[index].entry_id;

    for (i = 0; i < *selected_index; i++) {
        if (entries[i] == -1) {
            found_end = 1;
        }
    }

    if (entry_id == entries[*selected_index] && !found_end) {
        return;
    }

    for (i = 0; entries[i] != -1; i++) {
        if ((entries[i] & 0x3FF) == entry_id) {
            *selected_index = i;
            return;
        }
    }

    *selected_index = 0;
    *scroll_base_index = 0;
}
