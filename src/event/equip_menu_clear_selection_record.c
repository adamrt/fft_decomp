#include "fft/equip.h"
#include "psx/types.h"

/* EQUIP.OUT 001bfa44 - Clear the 6-byte record at `index` in the table at 0x801e12c4. */
void equip_menu_clear_selection_record(s32 index) {
    memset(&g_equip_menu_selection_records[index * 6], 0, 6);
}
