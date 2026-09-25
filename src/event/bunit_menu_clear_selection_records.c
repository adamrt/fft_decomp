#include "fft/event_bunit.h"
#include "psx/types.h"

void bunit_menu_clear_selection_records(void) {
    memset(
        g_bunit_menu_selection_records, 0, BUNIT_MENU_SELECTION_RECORD_COUNT * sizeof(bunit_menu_selection_record_t));
}
