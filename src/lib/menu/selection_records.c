/* Saved menu selections and row/scroll restoration. */

#include "fft/menu_selection.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Each menu program keeps its own remembered list positions: 14 in BUNIT and WORLD, 5 in EQUIP and JOBSTTS. */
static menu_selection_record_t s_menu_selection_records[VARIANT_MENU_SELECTION_RECORD_COUNT] = { 0 };

/* Clear every menu selection record. */
void menu_selection_clear_records(void) {
    memset(s_menu_selection_records, 0, sizeof(s_menu_selection_records));
}

/* Clear the 6-byte selection record at `index`. */
void menu_selection_clear_record(s32 index) {
    memset(&s_menu_selection_records[index], 0, sizeof(menu_selection_record_t));
}

/* Save a list's cursor row and scroll base in selection record `index`,
 * together with the id (low ten bits) of the entry under the cursor. */
void menu_selection_set_record(s32 index, s32 selected_index, s32 scroll_base_index, const s16* entries) {
    s_menu_selection_records[index].selected_index = selected_index;
    s_menu_selection_records[index].scroll_base_index = scroll_base_index;
    s_menu_selection_records[index].entry_id = entries[selected_index] & 0x3FF;
}

/* Keep the saved cursor when it still names the same entry; otherwise find
 * that entry or reset both positions. EQUIP and WORLD check for an earlier terminator.
 * BUNIT and JOBSTTS trust the saved cursor when its entry still matches. */
void menu_selection_restore_record(s32 index, s16* selected_index, s16* scroll_base_index, const s16* entries) {
    s32 entry_id;
#if defined(FFT_EVENT_EQUIP) || defined(FFT_WORLD)
    s32 found_end;
#endif
    s32 i;

    *selected_index = s_menu_selection_records[index].selected_index;
    *scroll_base_index = s_menu_selection_records[index].scroll_base_index;
    entry_id = s_menu_selection_records[index].entry_id;

#if defined(FFT_EVENT_EQUIP) || defined(FFT_WORLD)
    found_end = 0;
    for (i = 0; i < *selected_index; i++) {
        if (entries[i] == -1) {
            found_end = 1;
        }
    }
#endif
    if (entry_id == entries[*selected_index]
#if defined(FFT_EVENT_EQUIP) || defined(FFT_WORLD)
        && !found_end
#endif
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
