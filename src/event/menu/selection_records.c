/* 6-byte selection record clears, one and all. */

#include "fft/menu.h"
#include "fft/menu_input.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Clear every menu selection record. */
void menu_clear_selection_records(void) {
    memset(g_menu_selection_records, 0, VARIANT_MENU_SELECTION_RECORD_COUNT * sizeof(menu_selection_record_t));
}

/* Clear the 6-byte selection record at `index`. */
void menu_clear_selection_record(s32 index) {
    memset(&g_menu_selection_records[index], 0, sizeof(menu_selection_record_t));
}
