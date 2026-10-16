/* Saving a list's cursor row, scroll base and the entry under the cursor in a selection record. */

#include "fft/menu_selection.h"
#include "psx/types.h"

/* Save a list's cursor row and scroll base in selection record `index`,
 * together with the id (low ten bits) of the entry under the cursor. */
void menu_set_selection_record(s32 index, s32 selected_index, s32 scroll_base_index, s16* entries) {
    g_menu_selection_records[index].selected_index = selected_index;
    g_menu_selection_records[index].scroll_base_index = scroll_base_index;
    g_menu_selection_records[index].entry_id = entries[selected_index] & 0x3FF;
}
