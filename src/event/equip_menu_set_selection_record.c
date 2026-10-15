#include "fft/event_equip.h"
#include "psx/types.h"

/* Save a list's cursor row and scroll base in selection record `index`,
 * together with the id (low ten bits) of the entry under the cursor. */
void equip_menu_set_selection_record(s32 index, s32 selected_index, s32 scroll_index, s16* items) {
    g_menu_selection_records[index].selected_index = selected_index;
    g_menu_selection_records[index].scroll_base_index = scroll_index;
    g_menu_selection_records[index].entry_id = items[selected_index] & 0x3FF;
}
