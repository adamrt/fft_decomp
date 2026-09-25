#include "fft/event_bunit.h"
#include "psx/types.h"

/* BUNIT.OUT 001c65a8 - Clear the 6-byte record at `index` in the table at 0x801eb22c. */
void bunit_menu_clear_selection_record(s32 index) {
    memset(&g_bunit_menu_selection_records[index], 0, sizeof(bunit_menu_selection_record_t));
}
