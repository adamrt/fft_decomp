#include "fft/event_jobstts.h"

/* Save a list's cursor row and scroll base in selection record `index`,
 * together with the id (low ten bits) of the entry under the cursor. */
void jobstts_menu_set_selection_record(s32 index, s32 selected_index, s32 scroll_index, s16* abilities) {
    g_menu_selection_records[index].selected_index = selected_index;
    g_menu_selection_records[index].scroll_base_index = scroll_index;
    g_menu_selection_records[index].entry_id = abilities[selected_index] & 0x3ff;
}
