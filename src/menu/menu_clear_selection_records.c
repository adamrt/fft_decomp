#include "fft/menu.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Clear every saved menu selection record. The table has
 * VARIANT_MENU_SELECTION_RECORD_COUNT records: 14 in BUNIT and WORLD, 5 in
 * EQUIP and JOBSTTS. */
void menu_clear_selection_records(void) {
    memset(g_menu_selection_records, 0, VARIANT_MENU_SELECTION_RECORD_COUNT * sizeof(menu_selection_record_t));
}
