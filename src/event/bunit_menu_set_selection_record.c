#include "fft/event_bunit.h"
#include "psx/types.h"

/* BUNIT.OUT 001c65e4 - Populate the 6-byte record at `index` in the g_bunit_menu_selection_records
 * table with a source-id (low 10 bits masked from an indirection table). */
void bunit_menu_set_selection_record(s32 index, s32 field0, s32 field2, u16* lookup_table) {
    g_bunit_menu_selection_records[index].selected_index = field0;
    g_bunit_menu_selection_records[index].scroll_base_index = field2;
    g_bunit_menu_selection_records[index].entry_id = lookup_table[field0] & 0x3FF;
}
