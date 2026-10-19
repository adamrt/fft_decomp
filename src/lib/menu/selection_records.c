/* Saved menu selections and row/scroll restoration. */

#include "fft/menu_selection.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Each menu program keeps its own remembered list positions. */
menu_selection_record_t g_menu_selection_records[VARIANT_MENU_SELECTION_RECORD_COUNT] = { 0 };

/* Clear every menu selection record. */
void menu_clear_selection_records(void) {
    memset(g_menu_selection_records, 0, sizeof(g_menu_selection_records));
}

/* Clear the 6-byte selection record at `index`. */
void menu_clear_selection_record(s32 index) {
    memset(&g_menu_selection_records[index], 0, sizeof(menu_selection_record_t));
}

/* Save a list's cursor row and scroll base in selection record `index`,
 * together with the id (low ten bits) of the entry under the cursor. */
void menu_set_selection_record(s32 index, s32 selected_index, s32 scroll_base_index, const s16* entries) {
    g_menu_selection_records[index].selected_index = selected_index;
    g_menu_selection_records[index].scroll_base_index = scroll_base_index;
    g_menu_selection_records[index].entry_id = entries[selected_index] & 0x3FF;
}

/* WORLD restores selections in its selected-unit feature. */
#ifndef FFT_WORLD
/* Keep the saved cursor when it still names the same entry; otherwise find
 * that entry or reset both positions. EQUIP checks for an earlier terminator.
 * BUNIT and JOBSTTS trust the saved cursor when its entry still matches. */
void menu_get_selection_record(s32 index, s16* selected_index, s16* scroll_base_index, const s16* entries) {
    s32 entry_id;
    s32 i;
#    ifdef FFT_EVENT_EQUIP
    s32 found_end = 0;
#    endif

    *selected_index = g_menu_selection_records[index].selected_index;
    *scroll_base_index = g_menu_selection_records[index].scroll_base_index;
    entry_id = g_menu_selection_records[index].entry_id;

#    ifdef FFT_EVENT_EQUIP
    for (i = 0; i < *selected_index; i++) {
        if (entries[i] == -1) {
            found_end = 1;
        }
    }
#    endif
    if (entry_id == entries[*selected_index]
#    ifdef FFT_EVENT_EQUIP
        && !found_end
#    endif
    ) {
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
#endif
